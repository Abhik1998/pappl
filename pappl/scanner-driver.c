#include "pappl-private.h"
#include "printer-support.c"


#  define PAPPL_SCAN_INPUT_SOURCE_ADF 0x01
#  define PAPPL_SCAN_INPUT_SOURCE_PLATEN 0x04
#  define PAPPL_SCAN_COLOR_MODE_AUTO 0x01
#  define PAPPL_SCAN_COLOR_MODE_CMYK_8 0x800
//
// Local functions...
//

static ipp_t	*make_attrs_scan(pappl_system_t *system, pappl_sdriver_data_t *data);


//
// 'papplPrinterGetScanDriverData()' - Get the current print driver data.
//

pappl_sdriver_data_t *			// O - Driver data or `NULL` if none
papplPrinterGetScanDriverData(
    pappl_printer_t      *printer,	// I - Printer
    pappl_sdriver_data_t *data)		// I - Pointer to driver data structure to fill
{
  if (!printer || !printer->driver_name || !data)
  {
    if (data)
      _papplPrinterInitScanDriverData(data);

    return (NULL);
  }

  memcpy(data, &printer->psdriver.scan_driver_data, sizeof(pappl_sdriver_data_t));

  return (data);
}


//
// 'papplPrinterGetDriverName()' - Get the current driver name.
//

// char *					// O - Driver name or `NULL` for none
// papplPrinterGetDriverName(
//     pappl_printer_t *printer,		// I - Printer
//     char            *buffer,		// I - String buffer
//     size_t          bufsize)		// I - Size of string buffer
// {
//   if (!printer || !printer->driver_name || !buffer || bufsize == 0)
//   {
//     if (buffer)
//       *buffer = '\0';

//     return (NULL);
//   }

//   pthread_rwlock_rdlock(&printer->rwlock);
//   strlcpy(buffer, printer->driver_name, bufsize);
//   pthread_rwlock_unlock(&printer->rwlock);

//   return (buffer);
// }


//
// '_papplPrinterInitPrintDriverData()' - Initialize a print driver data structure.
//

void
_papplPrinterInitScanDriverData(
    pappl_sdriver_data_t *d)		// I - Driver data
{
    static const pappl_dither_t clustered = {};
  //gamma table


  memset(d, 0, sizeof(pappl_sdriver_data_t));
  memcpy(d->gdither, clustered, sizeof(d->gdither));
  memcpy(d->pdither, clustered, sizeof(d->pdither));

  d->orient_default  = IPP_ORIENT_NONE;
  d->content_default = PAPPL_CONTENT_AUTO;
  d->quality_default = IPP_QUALITY_NORMAL;
  d->sides_supported = PAPPL_SIDES_ONE_SIDED;
  d->sides_default   = PAPPL_SIDES_ONE_SIDED;
}


void
papplPrinterSetScanDriverData(
    pappl_printer_t      *printer,	// I - Printer
    pappl_sdriver_data_t *data,		// I - Driver data
    ipp_t                *attrs)	// I - Additional capability attributes or `NULL` for none
{
  if (!printer || !data)
    return;

  pthread_rwlock_wrlock(&printer->rwlock);

  // Copy driver data to scanner
  memcpy(&printer->psdriver.scan_driver_data, data, sizeof(printer->psdriver.scan_driver_data));

  // Create scanner (capability) attributes based on driver data...
  ippDelete(printer->driver_attrs);
  printer->driver_attrs = make_attrs_scan(printer->system, &printer->psdriver.scan_driver_data);

  if (attrs)
    ippCopyAttributes(printer->driver_attrs, attrs, 0, NULL, NULL);

  pthread_rwlock_unlock(&printer->rwlock);
}


static ipp_t *				// O - Driver attributes
make_attrs_scan(pappl_system_t       *system,// I - System
                pappl_sdriver_data_t *data)	// I - Driver data
{
  ipp_t			*attrs;		// Driver attributes
  unsigned		bit;		// Current bit value
  int			i, j,		// Looping vars
			num_values;	// Number of values
  const char		*svalues[100];	// String values
  int			ivalues[100];	// Integer values
  ipp_t			*cvalues[PAPPL_MAX_MEDIA * 2];
					// Collection values
  char			*ptr;		// Pointer into value
  const char		*prefix;	// Prefix string
  const char		*max_name = NULL,// Maximum size
		    	*min_name = NULL;// Minimum size
  ipp_attribute_t	*attr;		// Attribute


  static const char * const job_creation_attributes[] =
  {					// job-creation-attributes-supported values
    "compression-accepted",
    "document-data-wait",
    "document-format-accepted"
    "document-name",
    "input-attributes",
    "ipp-attribute-fidelity",
    "job-name",
    "output-attributes",
    "requesting-user-name"
    "requesting-user-uri"
    "destination-accesses",
    "copies",
    "destination-uris",
    "multiple-document-handling",
    "number-of-retries",
    "page-ranges",
    "retry-interval ",
    "retry-time-out"
  };
  static const char * const printer_settable_attributes[] =
  {					// scanner-settable-attributes values
    "copies-default",
    "document-format-default",
    "input-attributes-default",
    "number-of-retries-default",
    "output-attributes-default",
    "printer-geo-location",
    "printer-location",
    "printer-organization",
    "printer-organizational-unit",
    "retry-time-out-default",
    "retry-time-out-supported "
  };

  static const char * const input_attributes_supported[] =
  {					// input-attributes-supported values
    "input-auto-exposure",
    "input-auto-scaling",
    "input-auto-skew-correction",
    "input-brightness",
    "input-color-mode",
    "input-content-type",
    "input-contrast",
    "input-film-scan-mode",
    "input-images-to-transfer",
    "input-orientation-requested",
    "input-media",
    "input-media-type",
    "input-quality",
    "input-resolution",
    "input-scaling-height",
    "input-scaling-width",
    "input-scan-regions",
    "input-sharpness",
    "input-sides",
    "input-source"
  };

  static const char * const output_attributes_supported[] =
  {					// output-attributes-supported values
    "noise-removal",
    "output-compression-quality-factor"
  };

  // Create an empty IPP message for the attributes...
  attrs = ippNew();

  // color-supported
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "color-supported", data->ppm_color);

  // document-format-supported
  num_values = 0;
  svalues[num_values ++] = "application/pdf";
  svalues[num_values ++] = "image/jpeg";

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-supported", num_values, NULL, svalues);

  // identify-actions-supported
  for (num_values = 0, bit = PAPPL_IDENTIFY_ACTIONS_DISPLAY; bit <= PAPPL_IDENTIFY_ACTIONS_SPEAK; bit *= 2)
  {
    if (data->identify_supported & bit)
      svalues[num_values ++] = _papplIdentifyActionsString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-supported", num_values, NULL, svalues);


  // ipp-features-supported
  num_values = data->num_features;

  if (data->num_features > 0)
    memcpy(svalues, data->features, (size_t)data->num_features * sizeof(char *));

  svalues[num_values ++] = "ipp-everywhere";

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-features-supported", num_values, NULL, svalues);


  // job-creation-attributes-supported
  memcpy(svalues, job_creation_attributes, sizeof(job_creation_attributes));
  num_values = (int)(sizeof(job_creation_attributes) / sizeof(job_creation_attributes[0]));

  for (i = 0; i < data->num_vendor && i < (int)(sizeof(svalues) / sizeof(svalues[0])); i ++)
    svalues[num_values ++] = data->vendor[i];

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-creation-attributes-supported", num_values, NULL, svalues);


  // landscape-orientation-requested-preferred
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "landscape-orientation-requested-preferred", IPP_ORIENT_LANDSCAPE);


  // input-media-supported
  if (data->num_media)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-media-supported", data->num_media, NULL, data->media);

  // input-attributes-supported
  memcpy(svalues, input_attributes_supported, sizeof(input_attributes_supported));
  num_values = (int)(sizeof(input_attributes_supported) / sizeof(input_attributes_supported[0]));
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-attributes-supported", num_values, NULL, svalues);


  // output-attributes-supported
  memcpy(svalues, output_attributes_supported, sizeof(output_attributes_supported));
  num_values = (int)(sizeof(output_attributes_supported) / sizeof(output_attributes_supported[0]));
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-attributes-supported", num_values, NULL, svalues);
 

  // input-color-mode-supported
  for (num_values = 0, bit = PAPPL_SCAN_COLOR_MODE_AUTO; bit <= PAPPL_SCAN_COLOR_MODE_CMYK_8; bit *= 2)
  {
    if (bit & data->color_supported)
      svalues[num_values ++] = _papplColorModeString(bit);
  }
  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-color-mode-supported", num_values, NULL, svalues);

  // input-source-supported
  for (num_values = 0, bit = PAPPL_SCAN_INPUT_SOURCE_ADF; bit <= PAPPL_SCAN_INPUT_SOURCE_PLATEN; bit *= 2)
  {
    if (bit & data->kind)
      svalues[num_values ++] = _papplSourceString(bit);
  }
  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-source-supported", num_values, NULL, svalues);


  // printer-make-and-model
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, data->make_and_model);


  // input-resolution-supported
  if (data->num_resolution > 0)
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "input-resolution-supported", data->num_resolution, IPP_RES_PER_INCH, data->x_resolution, data->y_resolution);


  // printer-settable-attributes
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-settable-attributes", (int)(sizeof(printer_settable_attributes) / sizeof(printer_settable_attributes[0])), NULL, printer_settable_attributes);


  // input-sides-supported
  if (data->sides_supported)
  {
    for (num_values = 0, bit = PAPPL_SIDES_ONE_SIDED; bit <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; bit *= 2)
    {
      if (data->sides_supported & bit)
	    svalues[num_values ++] = _papplSidesString(bit);
    }
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-sides-supported", num_values, NULL, svalues);
  }
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "input-sides-supported", NULL, "one-sided");

  return (attrs);
}
