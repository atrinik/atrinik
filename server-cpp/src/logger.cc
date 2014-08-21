/*******************************************************************************
 *               Atrinik, a Multiplayer Online Role Playing Game               *
 *                                                                             *
 *       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team       *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program; if not, write to the Free Software Foundation, Inc.,     *
 * 675 Mass Ave, Cambridge, MA 02139, USA.                                     *
 *                                                                             *
 * The author can be reached at admin@atrinik.org                              *
 ******************************************************************************/

/**
 * @file
 * Logger implementation.
 */

#include <fstream>
#include <boost/ref.hpp>
#include <boost/bind.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/log/core.hpp>
#include <boost/log/common.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core/record.hpp>
#include <boost/log/attributes/value_visitation.hpp>
#include <boost/log/utility/setup/from_stream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

ostream& operator<<(ostream& strm, Logger::Level level)
{
    static const char* strings[] = {
        "development",
        "detail",
        "info",
        "error",
        "critical"
    };

    if (static_cast<size_t> (level) < sizeof (strings) / sizeof (*strings)) {
        strm << strings[level];
    } else {
        strm << static_cast<int> (level);
    }

    return strm;
}

struct scope_list_formatter {
    typedef void result_type;
    typedef log::attributes::named_scope::value_type scope_stack;

    explicit scope_list_formatter(log::attribute_name const& name) :
    name_(name)
    {
    }

    void operator()(log::record_view const& rec,
            log::formatting_ostream& strm) const
    {
        log::visit<scope_stack>
                (
                name_, rec.attribute_values(),
                bind(&scope_list_formatter::format, _1, boost::ref(strm))
                );
    }
private:

    static void format(scope_stack const& scopes, log::formatting_ostream& strm)
    {
        scope_stack::const_iterator it = scopes.begin(), end = scopes.end();

        for (; it != end; ++it) {
            strm << "\t" << it->scope_name << " [" << it->file_name << ":" <<
                    it->line << "]\n";
        }
    }
private:
    log::attribute_name name_;
};

class scope_formatter_factory : public log::formatter_factory<char> {
public:

    formatter_type create_formatter(log::attribute_name const& attr_name,
            args_map const& args)
    {
        return formatter_type(scope_list_formatter(attr_name));
    }
};

Logger::LoggerType Logger::logger;

void Logger::init()
{
    log::register_formatter_factory("Scope",
            make_shared<scope_formatter_factory>());
    log::register_simple_formatter_factory<Level, char>("Severity");

    ifstream settings("logging.conf");

    if (!settings.is_open()) {
        std::cout << "Could not open logging.conf file" << std::endl;
        return;
    }

    log::init_from_stream(settings);
    log::add_common_attributes();

    log::core::get()->add_global_attribute("Scope",
            log::attributes::named_scope());
    log::core::get()->add_global_attribute("ThreadID",
            log::attributes::current_thread_id());
}

};
