//
// Private printer header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_PRINTER_PRIVATE_H_
#  define _PAPPL_PRINTER_PRIVATE_H_

//
// Include necessary headers...
//

#  include "dnssd-private.h"
#  include "printer.h"
#  include "log.h"
#  include <grp.h>
#  ifdef __APPLE__
#    include <sys/param.h>
#    include <sys/mount.h>
#  else
#    include <sys/statfs.h>
#  endif // __APPLE__
#  ifdef HAVE_SYS_RANDOM_H
#    include <sys/random.h>
#  endif // HAVE_SYS_RANDOM_H
#  ifdef HAVE_GNUTLS_RND
#    include <gnutls/crypto.h>
#  endif // HAVE_GNUTLS_RND


//
// Include necessary headers...
//

#  include "base-private.h"
#  include "device.h"


extern void		_papplScannerWebCancelAllJobs(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebCancelJob(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebConfig(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebConfigFinalize(pappl_scanner_t *printer, int num_form, cups_option_t *form) _PAPPL_PRIVATE;
extern void		_papplScannerWebDefaults(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebDelete(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebHome(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebIteratorCallback(pappl_scanner_t *Scanner, pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplScannerWebJobs(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;
extern void		_papplScannerWebMedia(pappl_client_t *client, pappl_scanner_t *Scanner) _PAPPL_PRIVATE;