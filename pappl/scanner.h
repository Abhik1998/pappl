//
// Printer header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
#ifndef _PAPPL_PRINTER_H_
#  define _PAPPL_PRINTER_H_


//
// Include necessary headers...
//

#  include "base.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


#  define PAPPL_MAX_BIN		16	// Maximum number of output bins
#  define PAPPL_MAX_COLOR_MODE	8	// Maximum number of color modes
#  define PAPPL_MAX_MEDIA	256	// Maximum number of media sizes
#  define PAPPL_MAX_RASTER_TYPE	16	// Maximum number of raster types
#  define PAPPL_MAX_RESOLUTION	4	// Maximum number of printer resolutions
#  define PAPPL_MAX_SOURCE	16	// Maximum number of sources/rolls
#  define PAPPL_MAX_SUPPLY	32	// Maximum number of supplies
#  define PAPPL_MAX_TYPE	32	// Maximum number of media types
#  define PAPPL_MAX_VENDOR	32	// Maximum number of vendor extension attributes



enum pappl_scan_color_modes			// IPP "input-color-mode" bit values extended for
{
  PAPPL_SCAN_COLOR_MODE_AUTO = 0x01,			// 'auto': 
  PAPPL_SCAN_COLOR_MODE_BILEVEL = 0x02,		// 'bi-level': 
  PAPPL_SCAN_COLOR_MODE_COLOR = 0x04,			// 'color': 
  PAPPL_SCAN_COLOR_MODE_MONO_4= 0x08,			// 'monochrome_4': 
  PAPPL_SCAN_COLOR_MODE_MONO_8 = 0x10,		// 'monochrome_8': 
  PAPPL_SCAN_COLOR_MODE_MONO_16 = 0x20,			// 'monochrome_16':
  PAPPL_SCAN_COLOR_MODE_MONO = 0x40,		// 'monochrome':
  PAPPL_SCAN_COLOR_MODE_COLOR_8 = 0x80,			// 'color_8': 
  PAPPL_SCAN_COLOR_MODE_RGBA_8 = 0x100,			// 'rgba_8': 
  PAPPL_SCAN_COLOR_MODE_RGB_16 = 0x200,			// 'rgb_16':
  PAPPL_SCAN_COLOR_MODE_RGBA_16 = 0x400,			// 'rgba_16': 
  PAPPL_SCAN_COLOR_MODE_CMYK_8 = 0x800,		// 'cmyk_8': 
  PAPPL_SCAN_COLOR_MODE_CMYK_16 = 0x1000,			// 'cmyk_16': 
};
typedef unsigned pappl_scan_color_modes_t;
enum pappl_scan_content_type			// IPP "input-content-type" bit values
{
  PAPPL_SCAN_CONTENT_TYPE_AUTO = 0x01,			// 'auto': automatically determine the type of document
  PAPPL_SCAN_CONTENT_TYPE_HALFTONE = 0x02,		// 'halftone': automatically determine the type of document
  PAPPL_SCAN_CONTENT_TYPE_LINEART = 0x04,			// 'line-art': the document contains line art
  PAPPL_SCAN_CONTENT_TYPE_MAGAZINE= 0x08,			// 'magazine': the document is a magazine
  PAPPL_SCAN_CONTENT_TYPE_PHOTO = 0x10,			// 'photo': the document is a photograph
  PAPPL_SCAN_CONTENT_TYPE_TEXT = 0x20,			// 'text': the document only contains text
  PAPPL_SCAN_CONTENT_TYPE_TEXT_PHOTO = 0x40,		// 'text-and-photo': the document contains a combination of text and photographs
};
typedef unsigned pappl_scan_content_type_t;	// Bitfield for IPP "input-content-type" values for scan

enum pappl_scan_film			// IPP "input-film-scan-mode" bit values
{
  PAPPL_SCAN_FILM_BW_NEG = 0x01,			// 'black-and-white-negative-film': The film is black-and-white negatives
  PAPPL_SCAN_FILM_COLOR_NEG = 0x02,		// 'color-negative-film': The film is color negatives
  PAPPL_SCAN_FILM_COLOR_SLIDE = 0x04,			// 'color-slide-film': The film is color slides (positives)
  PAPPL_SCAN_FILM_NA = 0x08,			// 'not-applicable': The type of film is not applicable to the usage
};
typedef unsigned pappl_scan_film_t;	// Bitfield for IPP "input-film-scan-mode" values for scan

typedef struct pappl_scan_region_s		// "input-scan-regions" values
{
  int   x_origin;   // "x-origin" values in 1/2540th of an inch.
  int   x_dim;   // "x-dim" values in 1/2540th of an inch.
  int   y_origin;   // "y-origin" values in 1/2540th of an inch.
  int   y_dim;   // "y-dim" values in 1/2540th of an inch.
} pappl_scan_region_t;

enum pappl_scan_input_source			// IPP "input-source" bit values
{
  PAPPL_SCAN_INPUT_SOURCE_ADF = 0x01,			// 'adf': scans documents from the auto-document feeder
  PAPPL_SCAN_INPUT_SOURCE_FILM_READER = 0x02,		// 'film-reader': scans documents from a microfilm reader
  PAPPL_SCAN_INPUT_SOURCE_PLATEN = 0x04,			// 'platen': scans a single page document from the scanner glass or platen
};
typedef unsigned pappl_scan_input_source_t;	// Bitfield for IPP "input-film-scan-mode" values for scan

typedef struct pappl_scan_region_s		// "input-scan-regions" values
{
  int   x_origin;   // "x-origin" values in 1/2540th of an inch.
  int   x_dim;   // "x-dim" values in 1/2540th of an inch.
  int   y_origin;   // "y-origin" values in 1/2540th of an inch.
  int   y_dim;   // "y-dim" values in 1/2540th of an inch.
} pappl_scan_region_t;


struct pappl_pdriver_data_s		// Print driver data
//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_PRINTER_H_
