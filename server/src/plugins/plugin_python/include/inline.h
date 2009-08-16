
/*****************************************************************************/
/* Inline HOOK functions                                                     */
/*****************************************************************************/

/* START several inline HOOK functions */

/* we don't have a hook for query_name() - we assume that this
 * function only collect information and don't set/use any static
 * reference from crosslib.a
 */

static inline const char *add_string_hook(const char *stxt)
{
	CFParm* CFR;

	GCFP.Value[0] = (void *)(stxt);
	CFR=(PlugHooks[HOOK_ADDSTRING])(&GCFP);

	return (const char *)CFR->Value[0];
}

#define FREE_STRING_HOOK(_txt_) free_string_hook(_txt_);_txt_=NULL;
static inline void free_string_hook(const char *stxt)
{
	GCFP.Value[0] = (void *)(stxt);
	(PlugHooks[HOOK_FREESTRING])(&GCFP);
}

static inline void fix_player_hook(object *fp1)
{
	if (fp1->type == PLAYER)
	{
		GCFP.Value[0] = (void *)(fp1);
		(PlugHooks[HOOK_FIXPLAYER])(&GCFP);
	}
	else if (IS_LIVE(fp1))
	{
		/* if needed we should insert here fix monster! */
	}
}

static inline object *insert_ob_in_ob_hook(object *ob1, object *ob2)
{
	CFParm* CFR;

	GCFP.Value[0] = (void *)(ob1);
	GCFP.Value[1] = (void *)(ob2);
	CFR=(PlugHooks[HOOK_INSERTOBJECTINOB])(&GCFP);

	return (object *)CFR->Value[0];
}

/* END inline HOOK functions */
