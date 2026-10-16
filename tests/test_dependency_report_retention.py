"""Exercise the workflow's real inline retention code with synthetic report data.

The runner fixture models its documented per-line masking boundary; it does not
implement or certify the runner's secret registry or encoders. No network,
checkout, subprocess, credentials, or live runner is required.
"""

import contextlib
import io
import os
from pathlib import Path
import re
import stat
from types import SimpleNamespace
import tempfile
import textwrap
import unittest
from unittest.mock import patch


WORKFLOW = Path(__file__).resolve().parents[1] / '.github/workflows/dependency-review.yml'
LIMIT = 1024 * 1024
SECRET = 'synthetic-registered-token'


def script(name):
    source = WORKFLOW.read_text(encoding='utf-8')
    step = source.split('      - name: ' + name + '\n', 1)[1].split('      - name:', 1)[0]
    return textwrap.dedent(re.split(r'        run: (?:&\w+ )?\|\n', step, maxsplit=1)[1])


PREPARE = script('Prepare oversized dependency report')
EMIT = script('Mask dependency report part 01')
ASSEMBLE = script('Assemble runner-masked dependency report')


class DependencyReportRetentionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.workspace = self.root / 'workspace'
        self.workspace.mkdir()
        self.commands = self.root / '_runner_file_commands'
        self.commands.mkdir()
        self.environment = {
            'GITHUB_WORKSPACE': str(self.workspace),
            'RUNNER_TEMP': str(self.root),
            'GITHUB_OUTPUT': str(self.root / 'outputs'),
            'GITHUB_STEP_SUMMARY': str(self.commands / 'step_summary_prepare'),
        }
        self.output = io.StringIO()

    def execute(self, source, **environment):
        with patch.dict(os.environ, self.environment | environment), contextlib.redirect_stdout(self.output):
            exec(compile(source, 'workflow fixture', 'exec'), {'__name__': '__main__'})

    def prepare(self, data):
        if data is not None:
            (self.workspace / 'summary.md').write_bytes(data)
        self.execute(PREPARE)
        return int((self.root / 'outputs').read_text().rsplit('count=', 1)[1])

    def emit(self, count, missing=None):
        for number in range(1, count + 1):
            summary = self.commands / f'step_summary_part_{number:02d}'
            self.execute(EMIT, REPORT_PART=str(number), GITHUB_STEP_SUMMARY=str(summary))
            raw = summary.read_bytes()
            self.assertLessEqual(len(raw), LIMIT)
            if number != missing:
                # Verified runner source masks each complete CR/LF-delimited line.
                # Substitution is only a fixture for an already registered fake token.
                lines = re.split(r'\r\n|\r|\n', raw.decode('utf-8-sig'))
                if lines[-1] == '':
                    lines.pop()
                scrubbed = '\n'.join(line.replace(SECRET, '***') for line in lines) + '\n'
                Path(str(summary) + '-scrubbed').write_text(scrubbed, encoding='utf-8')

    def assemble(self, count):
        self.execute(ASSEMBLE, REPORT_PART_COUNT=str(count),
                     GITHUB_STEP_SUMMARY=str(self.commands / 'step_summary_assemble'))
        return self.root / 'dependency-review-retained/report.md'

    def assert_unknown(self):
        self.assertIn('unknown, not clean', self.output.getvalue())
        self.assertFalse((self.root / 'dependency-review-retained/report.md').exists())

    def test_full_oversized_report_is_masked_and_ordered(self):
        lines = [f'row {number:05d} {SECRET} ' + 'x' * 140 + '\r\n' for number in range(9000)]
        data = ''.join(lines).encode()
        self.assertGreater(len(data), LIMIT)  # The pinned action's actual large-report branch.
        count = self.prepare(data)
        self.assertGreater(count, 1)
        self.emit(count)
        report = self.assemble(count).read_text()
        self.assertNotIn(SECRET, report)
        self.assertEqual(report.count('***'), len(lines))
        self.assertEqual(re.findall(r'row ([0-9]{5})', report),
                         [f'{number:05d}' for number in range(len(lines))])
        for number in range(1, count + 1):
            self.assertIn(f'## Dependency review report part {number:02d}', report)
        self.assertIn('seven-day', (self.commands / 'step_summary_assemble').read_text())

    def test_complete_lines_keep_secret_and_unicode_separators_together(self):
        line = 'prefix\u0085\u2028\v\f' + SECRET + '\ufeffsuffix\r'
        data = (('x' * (LIMIT - 200) + '\r\n') + line + 'z' * 300 + '\n').encode()
        count = self.prepare(data)
        chunks = sorted((self.root / 'dependency-review-report-parts').glob('part-*.md'))
        self.assertEqual(sum(SECRET.encode() in part.read_bytes() for part in chunks), 1)
        self.assertTrue(any(line[:-1].encode() in part.read_bytes() for part in chunks))
        self.emit(count)
        self.assertNotIn(SECRET, self.assemble(count).read_text())

    def test_bom_at_second_part_start_is_data_after_trusted_heading(self):
        heading = b'## Dependency review report part 01\n\n'
        first = b'x' * (LIMIT - len(heading) - 1) + b'\n'
        data = b'\xef\xbb\xbf' + first + ('\ufeff' + SECRET + 'z' * 100 + '\n').encode()
        count = self.prepare(data)
        self.assertEqual(count, 2)
        second = (self.root / 'dependency-review-report-parts/part-02.md').read_bytes()
        self.assertTrue(second.startswith(b'## Dependency review report part 02\n\n'))
        self.assertIn(('\ufeff' + SECRET).encode(), second)
        self.emit(count)
        self.assertNotIn(SECRET, self.assemble(count).read_text())

    def test_absent_or_small_report_needs_no_large_report_parts(self):
        self.assertEqual(self.prepare(None), 0)
        self.assertEqual(self.output.getvalue(), '')
        self.assertFalse((self.root / 'dependency-review-report-parts').exists())
        self.assertEqual(self.prepare(b'small synthetic report\n'), 0)
        self.assertNotIn('clean', self.output.getvalue())

    def test_malformed_utf8_fails_before_any_part_is_published(self):
        self.assertEqual(self.prepare(b'x\n' * (LIMIT // 2) + b'\xff'), 0)
        self.assert_unknown()
        self.assertFalse((self.root / 'dependency-review-report-parts').exists())

    def test_oversized_complete_line_is_not_split(self):
        self.assertEqual(self.prepare(b'x' * (LIMIT + 1)), 0)
        self.assert_unknown()
        self.assertFalse((self.root / 'dependency-review-report-parts').exists())

    def test_read_and_part_count_limits_are_explicit_unknown(self):
        namespace = {'__name__': 'fixture'}
        exec(compile(PREPARE, 'workflow fixture', 'exec'), namespace)
        # Even under the 16 MiB read bound, whole-line packing can exceed 16 parts.
        with self.assertRaises(ValueError):
            namespace['plan_parts']((b'x' * (LIMIT // 2) + b'\n') * 17)
        self.assertEqual(self.prepare(b'x\n' * (16 * LIMIT // 2 + 1)), 0)
        self.assert_unknown()
        self.assertFalse((self.root / 'dependency-review-report-parts').exists())

    def test_actual_read_bound_is_checked_even_if_stat_size_is_stale(self):
        info = SimpleNamespace(st_mode=stat.S_IFREG, st_size=0)
        with patch('os.fstat', return_value=info):
            self.assertEqual(self.prepare(b'x' * (16 * LIMIT + 1)), 0)
        self.assert_unknown()
        self.assertFalse((self.root / 'dependency-review-report-parts').exists())

    def test_multibyte_line_limit_counts_utf8_bytes(self):
        self.assertEqual(self.prepare(('🦄' * (LIMIT // 4 + 1)).encode()), 0)
        self.assert_unknown()
        self.assertFalse((self.root / 'dependency-review-report-parts').exists())

    def test_report_symlink_and_fifo_are_rejected_without_reading_target(self):
        target = self.root / 'other-data'
        target.write_bytes(b'x\n' * LIMIT)
        report = self.workspace / 'summary.md'
        report.symlink_to(target)
        self.assertEqual(self.prepare(None), 0)
        self.assert_unknown()
        report.unlink()
        os.mkfifo(report)
        self.assertEqual(self.prepare(None), 0)
        self.assert_unknown()

    def test_missing_scrubbed_part_does_not_publish_partial_report(self):
        count = self.prepare(b'x\n' * LIMIT)
        self.emit(count, missing=count)
        self.assertFalse(self.assemble(count).exists())
        self.assert_unknown()

    def test_scrubbed_symlink_or_wrong_part_heading_is_rejected(self):
        count = self.prepare(b'x\n' * LIMIT)
        self.emit(count)
        scrubbed = self.commands / 'step_summary_part_01-scrubbed'
        scrubbed.write_text('## Dependency review report part 02\n\nwrong\n')
        self.assertFalse(self.assemble(count).exists())
        self.assert_unknown()
        scrubbed.unlink()
        scrubbed.symlink_to(self.workspace / 'summary.md')
        self.assertFalse(self.assemble(count).exists())
        self.assert_unknown()

    def test_missing_or_untrusted_path_record_rejects_complete_report(self):
        count = self.prepare(b'x\n' * LIMIT)
        self.emit(count)
        record = self.root / 'dependency-review-report-parts/scrubbed-01.path'
        record.write_text(str(self.workspace / 'summary.md'))
        self.assertFalse(self.assemble(count).exists())
        self.assert_unknown()
        record.unlink()
        record.symlink_to(self.workspace / 'summary.md')
        self.assertFalse(self.assemble(count).exists())
        self.assert_unknown()

    def test_failed_atomic_publication_leaves_no_partial_combined_report(self):
        count = self.prepare(b'x\n' * LIMIT)
        self.emit(count)
        with patch.object(Path, 'rename', side_effect=OSError('synthetic publication failure')):
            self.assertFalse(self.assemble(count).exists())
        self.assert_unknown()

    def test_policy_pins_and_upload_are_bounded_to_masked_files(self):
        workflow = WORKFLOW.read_text()
        self.assertRegex(workflow, r'actions/dependency-review-action@[0-9a-f]{40}\b')
        self.assertRegex(workflow, r'actions/upload-artifact@[0-9a-f]{40}\b')
        self.assertNotIn('ACTIONS_RUNTIME_TOKEN', workflow)
        self.assertIn('retention-days: 7', workflow)
        self.assertIn('step_summary_*-scrubbed', workflow)
        self.assertIn('dependency-review-retained/report.md', workflow)
        self.assertNotIn('github.workspace }}/summary.md', workflow)
        self.assertEqual(workflow.count('run: *mask_dependency_report_part'), 15)
        self.assertEqual(len(re.findall(r'- name: Mask dependency report part \d+', workflow)), 16)


if __name__ == '__main__':
    unittest.main()
