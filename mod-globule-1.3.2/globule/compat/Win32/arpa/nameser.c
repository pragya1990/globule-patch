#include <apr.h>
#include "compat/Win32/arpa/nameser.h"

/*
 * Expand compressed domain name 'src' to full domain name.
 * 'msg' is a pointer to the begining of the message,
 * 'eom' points to the first location after the message,
 * 'dst' is a pointer to a buffer of size 'dstsiz' for the result.
 * Return size of compressed name or -1 if there was an error.
 */
int
dn_expand(const apr_byte_t *msg, const apr_byte_t *eom, const apr_byte_t *src,
          char *dst, int dstsiz)
{
        int n = ns_name_uncompress(msg, eom, src, dst, (size_t) dstsiz);

        if (n > 0 && dst[0] == '.')
                dst[0] = '\0';
        return (n);
}

/*
 * Pack domain name 'exp_dn' in presentation form into 'comp_dn'.
 * Return the size of the compressed name or -1.
 * 'length' is the size of the array pointed to by 'comp_dn'.
 */
int
dn_comp(const char *src, apr_byte_t *dst, int dstsiz,
              apr_byte_t **dnptrs, apr_byte_t **lastdnptr)
{
        return (ns_name_compress(src, dst, (size_t)dstsiz,
                                 (const apr_byte_t **)dnptrs,
                                 (const apr_byte_t **)lastdnptr));
}

