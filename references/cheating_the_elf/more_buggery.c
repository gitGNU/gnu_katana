//from http://seclists.org/bugtraq/2002/May/249

/*
 * Copyright (C) 2002 the grugq.
 * All rights reserved.
 *
 * For educational purposes only.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <elf.h>
#define NULL    ((void *)0)

#define PAGE_SIZE       4096
// below is the real align(), but since we care about size, and we end up
// walking down and will likely discard the rounded up value
// we might as well use the cheat
// #define      ALIGN(k, v)     (((k) + ((v) - 1)) & ~((v) - 1))

#define PAGE_ALIGN(k)   ((k) & ~(PAGE_SIZE - 1)) // the cheat page_align()

/* a POSIX structure */
struct link_map {
        Elf32_Addr        l_addr;
        char            * l_name;
        Elf32_Dyn       * l_ld;         /* .dynamic ptr */
        struct link_map * l_next,
                        * l_prev;
};

struct resolv {
        Elf32_Word      * hash;
        Elf32_Word      * chain;
        Elf32_Sym       * symtab;
        char            * strtab;
        int               num;
};

static int
strmatch(const char *src1, const char *src2)
{
        register char   * s1 = src1,
                        * s2 = src2;

        for (; *s1 == *s2; s1++, s2++)
                if (*s1 == 0x00)
                        return (1);
        return (0);
}

static inline struct resolv *
build_resolv(struct link_map *l, struct resolv *r)
{
        Elf32_Dyn       * d;


        /* we could have an implicit test for 0, knowing the value of DT_NULL
         * but that isn't as clean, and the asm code doesn't change... */
        for (d = l->l_ld; d->d_tag != DT_NULL; d++) {
                switch (d->d_tag) {
                case DT_HASH:
                        {
                                Elf32_Word      * h;


                                /* whether an element is relocated seems to 
                                 * be an entirely platform dependant issue. */
                                h = (Elf32_Word *)
                                  ((char *)d->d_un.d_ptr);//+l->l_addr);

                                r->num = *h++; /* num buckets */
                                          h++; /* num chains */
                                r->hash = h;
                                r->chain = &h[r->num];
                        }
                        break;
                case DT_STRTAB:
                        r->strtab = (char *)d->d_un.d_ptr;
                        break;
                case DT_SYMTAB:
                        r->symtab = (Elf32_Sym *)d->d_un.d_ptr;
                        break;
                default:
                        break;
                }
        }
        return (r);
}

static void *
resolve(char *sym_name, long hn, struct link_map *l)
{
        Elf32_Sym       * sym;
        struct  resolv   r;
        long              ndx;

        memset(&r,0,sizeof(r));
        /* XXX error checking is a waste of space in a parasite; size matters */
        build_resolv(l,&r);

        for (ndx = r.hash[ hn % r.num ]; ndx; ndx = r.chain[ ndx ]) {
                sym = &r.symtab[ ndx ];

                /* this check is optional */
                if (ELF32_ST_TYPE(sym->st_info) != STT_FUNC)
                        continue;

                if (strmatch(sym_name, r.strtab + sym->st_name))
                        return (((char *)l->l_addr) + sym->st_value);
        }

        return NULL;
}

static struct link_map *
locate_link_map(void *my_base)
{
        Elf32_Ehdr      * e = (Elf32_Ehdr *)my_base;
        Elf32_Phdr      * p=0;
        Elf32_Dyn       * d=0;
        Elf32_Word      * got=0;


        p = (Elf32_Phdr *)((char *)e + e->e_phoff);

        while (p++<(Elf32_Phdr *)((char*)p + (e->e_phnum * sizeof(Elf32_Phdr))))
                if (p->p_type == PT_DYNAMIC)
                        break;

        /* XXX error checking, see above */
         
        if(p->p_type != PT_DYNAMIC)
                return (NULL);
        

        for (d = (Elf32_Dyn *)p->p_vaddr; d->d_tag != DT_NULL; d++)
                if (d->d_tag == DT_PLTGOT) {
                        got = (Elf32_Word *)d->d_un.d_ptr;
                        break;
                }
        /* XXX error check, see above */
        
         if (got == NULL)
                return (NULL);
        

        /* a platform dependant value. CPU architecture, not OS determined. */
#define GOT_LM_PTR      1
         return (struct link_map *)(*(got+GOT_LM_PTR));
}

void *
resolv(void *my_base, char *sym_name, long hn)
{
        struct link_map * l;
        void            * a;


        /* scan link maps... */
        l = locate_link_map(my_base);

        /* hope that the list is NULL terminated */
        while (l->l_prev)
        {
          puts("iterating backwards\n");
          l = l->l_prev;
        }
        
        /* scan link maps for the symbol. slow, but technically correct. */
        for (; l; l = l->l_next)
        {
          if(!strlen(l->l_name) && l->l_addr)
          {
            continue;//temporary workaround around an odd bug
          }
          if ((a = resolve(sym_name, hn, l)))
                        return (a);
        }

        return (NULL);
}

static inline void *
text_addr(void)
{
        void    * a;


        /* This isn't the cleanest way of getting an address that our code
         */
        __asm__ (
                "       call    .Lnext  \n"
                ".Lnext:                \n"
                "       pop     %%eax   \n"
                : "=a" (a)
                :
        );

        return ((void *)PAGE_ALIGN((long)a));
}

/* The value typically used on Linux i386 boxes is 0x08048000 */
static void *
locate_my_base(void)
{
        char    * a;


        /* We really should do a signal() for SIGSEGV and ensure that if we 
         * miss the ELF header (for some reason) we dont segfault the host
         * process image */
        for (a = text_addr();; a -= PAGE_SIZE) 
                if (!memcmp(a, ELFMAG, SELFMAG))
                        return (a);

        /* never reached. we either explode above, or get what we came for. */
        return (NULL);
}

/* With size considerations it makes more sense to hardcode the elf_hash()
 * value for a given symbol. This has the added benefit of improving
 * execution speed. */
unsigned long
elf_hash(const char *name)
{
        unsigned long   h = 0, g;

        while (*name) {
                h = (h << 4) + *name++;

                if ((g = h & 0xf0000000))
                        h ^= g >> 24;
                h &= ~g;
        }
        return (h);
}

int
main (void)
{
        unsigned long     hn;
        void    * my_addr;
        char    * sym_name = "printf";
        int     (*printf)(const char *, ...);


        hn = elf_hash(sym_name);
        my_addr = locate_my_base();
        printf = resolv(my_addr, sym_name, hn);

        (*printf)("Hello World\n");

        return (0);
}
