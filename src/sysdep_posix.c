/*
 * sysdep_posix.c - System-dependent interface for POSIX user-mode.
 */
/*
 * Copyright (c) 2010, Ideal World, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif /* HAVE_CONFIG_H */
#include "sysdep_posix.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const int omode2flags[] = {0, O_RDONLY | O_LARGEFILE,
                                  O_RDWR | O_LARGEFILE, O_WRONLY | O_LARGEFILE,
                                  O_RDWR | O_CREAT | O_LARGEFILE};

/*
 * Open a file handle and return a pointer to it.
 *
 * Parameters:
 * rhp   - Pointer to where to store pointer.
 * p     - Path to open.
 * flags - file control flags (see open(2))
 * mode  - file mode (see open(2))
 *
 * Returns:
 * - 0: Success.
 * - error: Otherwise (ENOMEM, see errno values of open(2)).
 */
static int
posix_open(void *rhp, const char *p, sysdep_open_mode_t omode) {
    int **fhpp = (int **)rhp;
    int * fhp;
    int   error = ENOMEM;
    if ((fhp = (int *)malloc(sizeof(int)))) {
        int flags = omode2flags[(int)omode];

        *fhp = open(p, flags, 0640);
        if (*fhp < 0) {
            error = errno;
            *fhpp = (int *)NULL;
            free(fhp);
        } else {
            *fhpp = fhp;
            error = 0;
        }
    }

    return error;
}

/*
 * Close a file handle and free pointer.
 *
 * Parameters:
 * rh - File handle
 *
 * Returns:
 * - 0: Success.
 * - EINVAL: Invalid file handle.
 * - error: Otherwise.
 */
static int
posix_closex(void *rh) {
    int *fhp   = (int *)rh;
    int  error = EINVAL;
    if (fhp) {
        error = close(*fhp);
        free(fhp);
    }

    return error;
}

/*
 * Seek to an offset in a file.
 *
 * Parameters:
 * rh      - File handle.
 * offset  - Offset to seek to.
 * whence  - One of SYSDEP_SEEK_ABSOLUTE, SYSDEP_SEEK_RELATIVE or
 * SYSDEP_SEEK_END.
 * resoffp - Pointer to resultant location (can be null).
 *
 * Returns:
 * - 0: Success.
 * - EINVAL: Invalid file handle.
 */
static int
posix_seek(void *rh, int64_t offset, sysdep_whence_t whence,
           uint64_t *resoffp) {
    int *fhp = (int *)rh;
    if (fhp) {
        off_t poffs = lseek(*fhp, offset, (int)whence);
        if (resoffp)
            *resoffp = (uint64_t)poffs;
        return (poffs >= 0) ? 0 : errno;
    } else {
        return EINVAL;
    }
}

/*
 * Read data from the current offset.
 *
 * Parameters:
 * rh  - File handle.
 * buf - Buffer to read into.
 * len - Length to read.
 * nr  - How many bytes read (written on success).
 *
 * Returns:
 * - 0: Success.
 * - EINVAL: Invalid file handle.
 * - error: Otherwise.
 */
static int
posix_read(void *rh, void *buf, uint64_t len, uint64_t *nr) {
    int *fhp = (int *)rh;
    if (fhp) {
        *nr = read(*fhp, buf, len);
        return (*nr == len) ? 0 : errno;
    } else {
        return EINVAL;
    }
}

/*
 * Write data at the current offset.
 *
 * Parameters:
 * rh  - File handle.
 * buf - Buffer to read into.
 * len - Length to read.
 * nw  - How many bytes written (written on success).
 *
 * Returns:
 * - 0: Success.
 * - EINVAL: Invalid file handle.
 * - error: Otherwise.
 */
static int
posix_write(void *rh, void *buf, uint64_t len, uint64_t *nw) {
    int *fhp = (int *)rh;
    if (fhp) {
        *nw = write(*fhp, buf, len);
        return (*nw == len) ? 0 : errno;
    } else {
        return EINVAL;
    }
}

/*
 * Allocate dynamic memory.
 *
 * Parameters:
 * nmpp   - Pointer to new memory pointer (written always).
 * nbytes - Size of memory to allocate.
 *
 * Returns:
 * - 0: Success.
 * - EINVAL: Invalid nmpp.
 * - ENOMEM: No memory available.
 */
static int
posix_malloc(void *nmpp, uint64_t nbytes) {
    void **xnmp = (void **)nmpp;
    return (xnmp) ? (((*xnmp = malloc(nbytes))) ? 0 : ENOMEM) : EINVAL;
}

/*
 * Free dynamic memory.
 *
 * Parameters:
 * mp - Pointer to memory.
 *
 * Returns:
 * - 0: Success.
 * - EINVAL: Invalid pointer.
 */
static int
posix_free(void *mp) {
    if (mp) {
        free(mp);
        return 0;
    } else {
        return EINVAL;
    }
}

/*
 * Determine a file's size.
 *
 * Paramters:
 *  rh     - Open file handle.
 *  nbytes - File size.
 */
static int
posix_file_size(void *rh, uint64_t *nbytes) {
    int  error = EINVAL;
    int *fhp   = (int *)rh;
    if (fhp) {
        struct stat sbuf;
        if ((error = fstat(*fhp, &sbuf)) == 0) {
            if (sbuf.st_size == 0) {
                uint64_t curpos;
                if (!posix_seek(rh, 0, SYSDEP_SEEK_RELATIVE, &curpos)) {
                    error = posix_seek(rh, 0, SYSDEP_SEEK_END, nbytes);
                    (void)posix_seek(rh, curpos, SYSDEP_SEEK_ABSOLUTE, NULL);
                }
            } else {
                *nbytes = sbuf.st_size;
            }
        }
    }

    return error;
}

const sysdep_dispatch_t posix_dispatch = {
    posix_open,  posix_closex, posix_seek, posix_read,
    posix_write, posix_malloc, posix_free, posix_file_size};
