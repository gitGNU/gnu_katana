/*
  File: unwind_dwarf_helper.c
  Author: James Oakley, but most code taken from dwarfdump and GPL licensed
  Project: Katana - preliminary testing
  Date: January 10
  Description: dwarf helper functions taken from dwarfdump
               they are not written by James Oakley
*/


#include <libdwarf.h>
#include <dwarf.h>
#include "util.h"

/*

A strcpy which ensures NUL terminated string
and never overruns the output.

*/
static void
safe_strcpy(char *out, long outlen, const char *in, long inlen)
{
    if (inlen >= (outlen - 1)) {
        strncpy(out, in, outlen - 1);
        out[outlen - 1] = 0;
    } else {
        strcpy(out, in);
    }
}


/*
    decode ULEB
*/
Dwarf_Unsigned local_dwarf_decode_u_leb128(unsigned char *leb128,
                            unsigned int *leb128_length)
{
    unsigned char byte = 0;
    Dwarf_Unsigned number = 0;
    unsigned int shift = 0;
    unsigned int byte_length = 1;

    byte = *leb128;
    for (;;) {
        number |= (byte & 0x7f) << shift;
        shift += 7;

        if ((byte & 0x80) == 0) {
            if (leb128_length != NULL)
                *leb128_length = byte_length;
            return (number);
        }

        byte_length++;
        byte = *(++leb128);
    }
}


/*
        Returns 1 if a proc with this low_pc found.
        Else returns 0.


*/
static int
get_proc_name(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr low_pc,
              char *proc_name_buf, int proc_name_buf_len)
{
    Dwarf_Signed atcnt = 0;
    Dwarf_Signed i = 0;
    Dwarf_Attribute *atlist = NULL;
    Dwarf_Addr low_pc_die = 0;
    int atres = 0;
    int funcres = 1;
    int funcpcfound = 0;
    int funcnamefound = 0;
    Dwarf_Error err;

    proc_name_buf[0] = 0;       /* always set to something */
    atres = dwarf_attrlist(die, &atlist, &atcnt, &err);
    if (atres == DW_DLV_ERROR) {
        death("dwarf_attrlist");
        return 0;
    }
    if (atres == DW_DLV_NO_ENTRY) {
        return 0;
    }
    for (i = 0; i < atcnt; i++) {
        Dwarf_Half attr;
        int ares;
        char* temps;
        int sres;
        int dres;

        if (funcnamefound == 1 && funcpcfound == 1) {
            /* stop as soon as both found */
            break;
        }
        ares = dwarf_whatattr(atlist[i], &attr, &err);
        if (ares == DW_DLV_ERROR) {
            death("get_proc_name whatattr error");
        } else if (ares == DW_DLV_OK) {
            switch (attr) {
            case DW_AT_specification:
            case DW_AT_abstract_origin:
                {
                    if(!funcnamefound) {
                        /* Only use this if we have not seen DW_AT_name
                           yet .*/
                        //int aores = get_abstract_origin_funcname(dbg,
                        //    atlist[i], proc_name_buf,proc_name_buf_len);
                      //if(aores == DW_DLV_OK) {
                      //      /* FOUND THE NAME */
                      //      funcnamefound = 1; 
                      //  }
                      death("can't deal with abstract origin funcname\n");
                        
                    }
                }
                break;
            case DW_AT_name:
                /* Even if we saw DW_AT_abstract_origin, go ahead
                   and take DW_AT_name. */
                sres = dwarf_formstring(atlist[i], &temps, &err);
                if (sres == DW_DLV_ERROR) {
                  death("formstring in get_proc_name failed");
                    /* 50 is safe wrong length since is bigger than the 
                       actual string */
                    safe_strcpy(proc_name_buf, proc_name_buf_len,
                                "ERROR in dwarf_formstring!", 50);
                } else if (sres == DW_DLV_NO_ENTRY) {
                    /* 50 is safe wrong length since is bigger than the 
                       actual string */
                    safe_strcpy(proc_name_buf, proc_name_buf_len,
                                "NO ENTRY on dwarf_formstring?!", 50);;
                } else {
                  long len = (long) strlen(temps);
                  safe_strcpy(proc_name_buf, proc_name_buf_len, temps,
                                len);
                }
                funcnamefound = 1;      /* FOUND THE NAME */
                break;
            case DW_AT_low_pc:
                dres = dwarf_formaddr(atlist[i], &low_pc_die, &err);
                if (dres == DW_DLV_ERROR) {
                  death("formaddr in get_proc_name failed");
                    low_pc_die = ~low_pc;
                    /* ensure no match */
                }
                funcpcfound = 1;

                break;
            default:
                break;
            }
        }
    }
    for (i = 0; i < atcnt; i++) {
        dwarf_dealloc(dbg, atlist[i], DW_DLA_ATTR);
    }
    dwarf_dealloc(dbg, atlist, DW_DLA_LIST);
    if (funcnamefound == 0 || funcpcfound == 0 || low_pc != low_pc_die) {
        funcres = 0;
    }
    return (funcres);
}


//this function copied verbatim from dwarfdump
//unlike the rest of the code in this file it was not written
//by James Oakley
/*
        Modified Depth First Search looking for the procedure:
        a) only looks for children of subprogram.
        b) With subprogram looks at current die *before* looking
           for a child.
        
        Needed since some languages, including SGI MP Fortran,
        have nested functions.
        Return 0 on failure, 1 on success.
*/
static int
get_nested_proc_name(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr low_pc,
                     char *ret_name_buf, int ret_name_buf_len)
{
    char name_buf[BUFSIZ];
    Dwarf_Die curdie = die;
    int die_locally_gotten = 0;
    Dwarf_Die prev_child = 0;
    Dwarf_Die newchild = 0;
    Dwarf_Die newsibling = 0;
    Dwarf_Half tag;
    Dwarf_Error err = 0;
    int chres = DW_DLV_OK;

    ret_name_buf[0] = 0;
    name_buf[0] = 0;
    while (chres == DW_DLV_OK) {
        int tres;

        tres = dwarf_tag(curdie, &tag, &err);
        newchild = 0;
        err = 0;
        if (tres == DW_DLV_OK) {
            int lchres;

            if (tag == DW_TAG_subprogram) {
                int proc_name_v = 0;

                proc_name_v = get_proc_name(dbg, curdie, low_pc,
                                            name_buf, BUFSIZ);
                if (proc_name_v) {
                    safe_strcpy(ret_name_buf, ret_name_buf_len,
                                name_buf, (long) strlen(name_buf));
                    if (die_locally_gotten) {
                        /* If we got this die from the parent, we do
                           not want to dealloc here! */
                        dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
                    }
                    return 1;
                }
                /* check children of subprograms recursively should
                   this really be check children of anything? */

                lchres = dwarf_child(curdie, &newchild, &err);
                if (lchres == DW_DLV_OK) {
                    /* look for inner subprogram */
                    int newprog =
                        get_nested_proc_name(dbg, newchild, low_pc,
                                             name_buf, BUFSIZ);

                    dwarf_dealloc(dbg, newchild, DW_DLA_DIE);
                    if (newprog) {
                        /* Found it.  We could just take this name or
                           we could concatenate names together For now, 
                           just take name */
                        if (die_locally_gotten) {
                            /* If we got this die from the parent, we
                               do not want to dealloc here! */
                            dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
                        }
                        safe_strcpy(ret_name_buf, ret_name_buf_len,
                                    name_buf, (long) strlen(name_buf));
                        return 1;
                    }
                } else if (lchres == DW_DLV_NO_ENTRY) {
                    /* nothing to do */
                } else {
                  death("get_nested_proc_name dwarf_child() failed ");
                    if (die_locally_gotten) {
                        /* If we got this die from the parent, we do
                           not want to dealloc here! */
                        dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
                    }
                    return 0;
                }
            }                   /* end if TAG_subprogram */
        } else {
            death("no tag on child read ");
            if (die_locally_gotten) {
                /* If we got this die from the parent, we do not want
                   to dealloc here! */
                dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
            }
            return 0;
        }
        /* try next sibling */
        prev_child = curdie;
        chres = dwarf_siblingof(dbg, curdie, &newsibling, &err);
        if (chres == DW_DLV_ERROR) {
          death("dwarf_cu_header On Child read ");
            if (die_locally_gotten) {
                /* If we got this die from the parent, we do not want
                   to dealloc here! */
                dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
            }
            return 0;
        } else if (chres == DW_DLV_NO_ENTRY) {
            if (die_locally_gotten) {
                /* If we got this die from the parent, we do not want
                   to dealloc here! */
                dwarf_dealloc(dbg, prev_child, DW_DLA_DIE);
            }
            return 0;           /* proc name not at this level */
        } else {                /* DW_DLV_OK */
            curdie = newsibling;
            if (die_locally_gotten) {
                /* If we got this die from the parent, we do not want
                   to dealloc here! */
                dwarf_dealloc(dbg, prev_child, DW_DLA_DIE);
            }
            prev_child = 0;
            die_locally_gotten = 1;
        }

    }
    if (die_locally_gotten) {
        /* If we got this die from the parent, we do not want to
           dealloc here! */
        dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
    }
    return 0;
}

Dwarf_Die current_cu_die_for_print_frames; /* This is
        an awful hack, making this public. But it enables
        cleaning up (doing all dealloc needed). */


char* get_fde_proc_name(Dwarf_Debug dbg, Dwarf_Addr low_pc)
{
    static char proc_name[BUFSIZ];
    Dwarf_Unsigned cu_header_length = 0;
    Dwarf_Unsigned abbrev_offset = 0;
    Dwarf_Half version_stamp = 0;
    Dwarf_Half address_size = 0;
    Dwarf_Unsigned next_cu_offset = 0;
    int cures = DW_DLV_OK;
    int dres = DW_DLV_OK;
    int chres = DW_DLV_OK;
    int looping = 0;
    Dwarf_Error err;

    proc_name[0] = 0;
    if (current_cu_die_for_print_frames == NULL) {
        /* Call depends on dbg->cu_context to know what to do. */
        cures = dwarf_next_cu_header(dbg, &cu_header_length,
                                   &version_stamp, &abbrev_offset,
                                   &address_size, &next_cu_offset,
                                   &err);
        if (cures == DW_DLV_ERROR) {
            return NULL;
        } else if (cures == DW_DLV_NO_ENTRY) {
            /* loop thru the list again */
            current_cu_die_for_print_frames = 0;
            ++looping;
        } else {                /* DW_DLV_OK */
            dres = dwarf_siblingof(dbg, NULL,
                                   &current_cu_die_for_print_frames,
                                   &err);
            if (dres == DW_DLV_ERROR) {
                return NULL;
            }
        }
    }
    if (dres == DW_DLV_OK) {
        Dwarf_Die child = 0;

        if (current_cu_die_for_print_frames == 0) {
            /* no information. Possibly a stripped file */
            return NULL;
        }
        chres =
            dwarf_child(current_cu_die_for_print_frames, &child, &err);
        if (chres == DW_DLV_ERROR) {
          death("dwarf_cu_header on child read ");
        } else if (chres == DW_DLV_NO_ENTRY) {
        } else {                /* DW_DLV_OK */
            int gotname =
                get_nested_proc_name(dbg, child, low_pc, proc_name,
                                     BUFSIZ);

            dwarf_dealloc(dbg, child, DW_DLA_DIE);
            if (gotname) {
                return (proc_name);
            }
            child = 0;
        }
    }
    for (;;) {
        Dwarf_Die ldie;

        cures = dwarf_next_cu_header(dbg, &cu_header_length,
                                     &version_stamp, &abbrev_offset,
                                     &address_size, &next_cu_offset,
                                     &err);

        if (cures != DW_DLV_OK) {
            break;
        }


        dres = dwarf_siblingof(dbg, NULL, &ldie, &err);

        if (current_cu_die_for_print_frames) {
            dwarf_dealloc(dbg, current_cu_die_for_print_frames,
                          DW_DLA_DIE);
        }
        current_cu_die_for_print_frames = 0;
        if (dres == DW_DLV_ERROR) {
          death("dwarf_cu_header Child Read finding proc name for .debug_frame");
            continue;
        } else if (dres == DW_DLV_NO_ENTRY) {
            ++looping;
            if (looping > 1) {
                death("looping  on cu headers!");
                return NULL;
            }
            continue;
        }
        /* DW_DLV_OK */
        current_cu_die_for_print_frames = ldie;
        {
            int chres;
            Dwarf_Die child;

            chres =
                dwarf_child(current_cu_die_for_print_frames, &child,
                            &err);
            if (chres == DW_DLV_ERROR) {
                death( "dwarf Child Read ");
            } else if (chres == DW_DLV_NO_ENTRY) {

                ;               /* do nothing, loop on cu */
            } else {            /* DW_DLV_OK) */

                int gotname =
                    get_nested_proc_name(dbg, child, low_pc, proc_name,
                                         BUFSIZ);

                dwarf_dealloc(dbg, child, DW_DLA_DIE);
                if (gotname) {
                    return (proc_name);
                }
            }
        }
    }
    return (NULL);
}
