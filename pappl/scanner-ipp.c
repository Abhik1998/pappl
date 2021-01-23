//
// Printer IPP processing for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"

//
// Local functions...
//
static void		ipp_scan_create_job(pappl_client_t *client);
static void   ipp_get_next_document_data(pappl_client_t *client);

//
// 'ipp_scan_create_job()' - Create a scan job object.
//
static void
ipp_scan_create_job(pappl_client_t *client)	// I - Client
{
  pappl_job_t		*job;		// New job
  cups_array_t		*ra;		// Attributes to send in response

  // Validate scan job attributes...
  if (!valid_job_attributes(client))
    return;

  // Create the job...
  if ((job = papplJobCreate(client)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

  // Return the job info...
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}

//
// 'ipp_validate_job()' - Validate job creation attributes.
//

static void
ipp_validate_job(
    pappl_client_t *client)		// I - Client
{
  if (valid_job_attributes(client))
    papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'valid_job_attributes()' - Determine whether the job attributes are valid.
//
// When one or more job attributes are invalid, this function adds a suitable
// response and attributes to the unsupported group.
//

static bool				// O - `true` if valid, `false` if not
valid_job_attributes(
    pappl_client_t *client)		// I - Client
{
  int			i,		// Looping var
			count;		// Number of values
  bool			valid = true;	// Valid attributes?
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute


  // If a shutdown is pending, do not accept more jobs...
  if (client->system->shutdown_time)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return (false);
  }

  // Check operation attributes...
  valid = _papplJobValidateDocumentAttributes(client);

  pthread_rwlock_rdlock(&client->printer->rwlock);

  // Check the various job template attributes...
  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "no-hold"))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "none"))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else
    {
      supported = ippFindAttribute(client->printer->driver_attrs, "media-supported", IPP_TAG_KEYWORD);

      if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col", IPP_TAG_ZERO)) != NULL)
  {
    ipp_t		*col,		// media-col collection
			*size;		// media-size collection
    ipp_attribute_t	*member,	// Member attribute
			*x_dim,		// x-dimension
			*y_dim;		// y-dimension
    int			x_value,	// y-dimension value
			y_value;	// x-dimension value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BEGIN_COLLECTION)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }

    col = ippGetCollection(attr, 0);

    if ((member = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetCount(member) != 1 || (ippGetValueTag(member) != IPP_TAG_NAME && ippGetValueTag(member) != IPP_TAG_NAMELANG && ippGetValueTag(member) != IPP_TAG_KEYWORD))
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	supported = ippFindAttribute(client->printer->driver_attrs, "media-supported", IPP_TAG_KEYWORD);

	if (!ippContainsString(supported, ippGetString(member, 0, NULL)))
	{
	  papplClientRespondIPPUnsupported(client, attr);
	  valid = false;
	}
      }
    }
    else if ((member = ippFindAttribute(col, "media-size", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      if (ippGetCount(member) != 1)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
      else
      {
	size = ippGetCollection(member, 0);

	if ((x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(x_dim) != 1 || (y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(y_dim) != 1)
	{
	  papplClientRespondIPPUnsupported(client, attr);
	  valid = false;
	}
	else
	{
	  x_value   = ippGetInteger(x_dim, 0);
	  y_value   = ippGetInteger(y_dim, 0);
	  supported = ippFindAttribute(client->printer->driver_attrs, "media-size-supported", IPP_TAG_BEGIN_COLLECTION);
	  count     = ippGetCount(supported);

	  for (i = 0; i < count ; i ++)
	  {
	    size  = ippGetCollection(supported, i);
	    x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_ZERO);
	    y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_ZERO);

	    if (ippContainsInteger(x_dim, x_value) && ippContainsInteger(y_dim, y_value))
	      break;
	  }

	  if (i >= count)
	  {
	    papplClientRespondIPPUnsupported(client, attr);
	    valid = false;
	  }
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || (strcmp(ippGetString(attr, 0, NULL), "separate-documents-uncollated-copies") && strcmp(ippGetString(attr, 0, NULL), "separate-documents-collated-copies")))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT || ippGetInteger(attr, 0) > IPP_ORIENT_NONE)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges", IPP_TAG_ZERO)) != NULL)
  {
    int upper = 0, lower = ippGetRange(attr, 0, &upper);
					// "page-ranges" value

    if (!ippGetBoolean(ippFindAttribute(client->printer->attrs, "page-ranges-supported", IPP_TAG_BOOLEAN), 0) || ippGetValueTag(attr) != IPP_TAG_RANGE || ippGetCount(attr) != 1 || lower < 1 || upper < lower)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-color-mode", IPP_TAG_ZERO)) != NULL)
  {
    pappl_color_mode_t value = _papplColorModeValue(ippGetString(attr, 0, NULL));
					// "print-color-mode" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !(value & client->printer->scan_driver_data.color_supported))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-content-optimize", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !_papplContentValue(ippGetString(attr, 0, NULL)))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-darkness", IPP_TAG_ZERO)) != NULL)
  {
    int value = ippGetInteger(attr, 0);	// "print-darkness" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || value < -100 || value > 100 || client->printer->scan_driver_data.darkness_supported == 0)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-quality", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT || ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-scaling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !_papplScalingValue(ippGetString(attr, 0, NULL)))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-speed", IPP_TAG_ZERO)) != NULL)
  {
    int value = ippGetInteger(attr, 0);	// "print-speed" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || value < client->printer->scan_driver_data.speed_supported[0] || value > client->printer->scan_driver_data.speed_supported[1] || client->printer->scan_driver_data.speed_supported[1] == 0)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution", IPP_TAG_ZERO)) != NULL)
  {
    int		xdpi,			// Horizontal resolution
		ydpi;			// Vertical resolution
    ipp_res_t	units;			// Resolution units

    xdpi  = ippGetResolution(attr, 0, &ydpi, &units);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_RESOLUTION || units != IPP_RES_PER_INCH)
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
    else
    {
      for (i = 0; i < client->printer->scan_driver_data.num_resolution; i ++)
      {
        if (xdpi == client->printer->scan_driver_data.x_resolution[i] && ydpi == client->printer->scan_driver_data.y_resolution[i])
          break;
      }

      if (i >= client->printer->scan_driver_data.num_resolution)
      {
	papplClientRespondIPPUnsupported(client, attr);
	valid = false;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    pappl_sides_t value = _papplSidesValue(ippGetString(attr, 0, NULL));
					// "sides" value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || !(value & client->printer->scan_driver_data.sides_supported))
    {
      papplClientRespondIPPUnsupported(client, attr);
      valid = false;
    }
  }

  pthread_rwlock_unlock(&client->printer->rwlock);

  return (valid);
}
//
// 'valid_scan_doc_attributes()' - Determine whether the document attributes are
//                                 valid for scanning.
//
// When one or more document attributes are invalid, this function adds a
// suitable response and attributes to the unsupported group.
//

static int				// O - 1 if valid, 0 if not
valid_scan_doc_attributes(
    pappl_client_t *client)		// I - Client
{
  int			valid = 1,	// Valid attributes?
      num_supported,  // Number of acceptable-xxx attribute values supplied by client
      i,    //looping var
      flag;   // flag var
  ipp_op_t		op = ippGetOperation(client->request);
					// IPP operation
  const char		*op_name = ippOpString(op);
					// IPP operation name
  ipp_attribute_t	*attr,		// Current attribute
      *inner_attr,    // looping var for collection of attributes
			*supported,	// xxx-supported attribute
      *inner_supported;	 // xxx-supported attribute
  ipp_t *coll;    // current collection
  const char		*compression = NULL,
					// compression value
			*format = NULL,	// document-format value
      *attr_string_val; // generic attribute string value


  // Check operation attributes...
  if ((attr = ippFindAttribute(client->request, "compression-accepted", IPP_TAG_ZERO)) != NULL)
  {
    // If acceptable compressions are specified, use the first supported value. If none of them is
    // supported, use "none"

    if (ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetGroupTag(attr) != IPP_TAG_OPERATION || (op != IPP_OP_CREATE_JOB && op != IPP_OP_VALIDATE_JOB))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    else
    {
      flag = 0;   // 0: no acceptable compression supported
      num_supported = ippGetCount(attr);
      supported   = ippFindAttribute(client->printer->attrs, "compression-supported", IPP_TAG_ZERO);

      for(i = 0; i < num_supported; i++)
      {
        compression = ippGetString(attr,i,NULL);
        if (ippContainsString(supported, compression))
        {
          flag = 1;   // acceptable compression found
          break;
        }
      }
      if (flag == 0)    // no acceptable compression found
      {
        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"compression\"='%s'", op_name, "none");
        ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, "none");           
      }
      else
      {
        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"compression\"='%s'", op_name, compression);
        ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, compression);

        if (strcmp(compression, "none"))
        {
          papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s \"compression\"='%s'", op_name, compression);
          httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
        }
      } 
    }
  }
  else
  {
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"compression\"='%s'", op_name, "none");
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, "none");    
  }


  if ((attr = ippFindAttribute(client->request, "document-format-accepted", IPP_TAG_ZERO)) != NULL)
  {
    // If acceptable document formats are specified, use the first supported value. If none of them is
    // supported, use the default one

    if (ippGetValueTag(attr) != IPP_TAG_MIMETYPE || ippGetGroupTag(attr) != IPP_TAG_OPERATION || (op != IPP_OP_CREATE_JOB && op != IPP_OP_VALIDATE_JOB))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    else
    {
      flag = 0;   // 0: no acceptable document format supported
      num_supported = ippGetCount(attr);
      supported   = ippFindAttribute(client->printer->driver_attrs, "document-format-supported", IPP_TAG_MIMETYPE);

      for(i = 0; i < num_supported; i++)
      {
        format = ippGetString(attr,i,NULL);
        if (ippContainsString(supported, format))
        {
          flag = 1;   // acceptable document format found
          break;
        }
      }
      if (flag == 0)    // no acceptable document format found
      {
        format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);          
      }
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"document-format\"='%s'", op_name, format);
      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format); 
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);    
    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"document-format\"='%s'", op_name, format);
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format);     
  }  

  if ((attr = ippFindAttribute(client->request, "document-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetValueTag(attr) != IPP_TAG_NAME || ippGetGroupTag(attr) != IPP_TAG_OPERATION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s \"document-name\"='%s'", op_name, format);
      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-name-supplied", NULL, ippGetName(attr)); 
    }
  }

  // Check if the supplied input attributes are supported
  // Note that input-attribute member attributes do not have a corresponding "-supplied" attribute defined.
  //  they will therefore be processed during job-processing

  if ((attr = ippFindAttribute(client->request, "input-attributes", IPP_TAG_ZERO)) != NULL)
  {
    coll = ippGetCollection(attr,0);
    bool    is_present_input_auto_exposure = (ippFindAttribute(coll, "input-auto-exposure", IPP_TAG_ZERO) != NULL),
      is_present_input_auto_scaling = (ippFindAttribute(coll, "input-auto-scaling", IPP_TAG_ZERO) != NULL),
      is_present_input_auto_skew_correction = (ippFindAttribute(coll, "input-auto-skew-correction", IPP_TAG_ZERO) != NULL),
      is_present_input_brightness = (ippFindAttribute(coll, "input-brightness", IPP_TAG_ZERO) != NULL),
      is_present_input_color_mode = (ippFindAttribute(coll, "input-color-mode", IPP_TAG_ZERO) != NULL),
      is_present_input_content_type = (ippFindAttribute(coll, "input-content-type", IPP_TAG_ZERO) != NULL),
      is_present_input_contrast = (ippFindAttribute(coll, "input-contrast", IPP_TAG_ZERO) != NULL),
      is_present_input_film_scan_mode = (ippFindAttribute(coll, "input-film-scan-mode", IPP_TAG_ZERO) != NULL),
      is_present_input_images_to_transfer = (ippFindAttribute(coll, "input-images-to-transfer", IPP_TAG_ZERO) != NULL), 
      is_present_input_orientation_requested = (ippFindAttribute(coll, "input-orientation-requested", IPP_TAG_ZERO) != NULL),
      is_present_input_media = (ippFindAttribute(coll, "input-media", IPP_TAG_ZERO) != NULL),
      is_present_input_quality = (ippFindAttribute(coll, "input-quality", IPP_TAG_ZERO) != NULL),
      is_present_input_resolution = (ippFindAttribute(coll, "input-resolution", IPP_TAG_ZERO) != NULL),
      is_present_input_scaling_height = (ippFindAttribute(coll,  "input-scaling-height", IPP_TAG_ZERO) != NULL),
      is_present_input_scaling_width = (ippFindAttribute(coll, "input-scaling-width", IPP_TAG_ZERO) != NULL),
      is_present_input_scan_regions = (ippFindAttribute(coll,  "input-scan-regions", IPP_TAG_ZERO) != NULL),
      is_present_input_sharpness = (ippFindAttribute(coll, "input-sharpness", IPP_TAG_ZERO) != NULL),
      is_present_input_sides = (ippFindAttribute(coll, "input-sides", IPP_TAG_ZERO) != NULL),
      is_present_input_source =    (ippFindAttribute(coll, "input-source", IPP_TAG_ZERO) != NULL);

    // input-brightness | input-contrast | input-sharpness cannot be present with input-auto-exposure
    if (is_present_input_auto_exposure && (is_present_input_brightness || is_present_input_contrast || is_present_input_sharpness))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "input-auto-exposure called with brightness|contrast| sharpness: op code %s ", op_name);
      respond_unsupported(client, ippFindAttribute(coll, "input-auto-exposure", IPP_TAG_ZERO));
      valid = 0;      
    }

    // input-brightness | input-contrast | input-sharpness cannot be present with input-auto-exposure
    if (is_present_input_auto_scaling && (is_present_input_scaling_height || is_present_input_scaling_width))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "input-auto-scaling called with scaling-height|scaling-width: op code %s ", op_name);
      respond_unsupported(client, ippFindAttribute(coll, "input-auto-scaling", IPP_TAG_ZERO));
      valid = 0;  
    }

    supported = ippFindAttribute(client->printer->driver_attrs, "input-attributes-supported", IPP_TAG_ZERO);
    num_supported = ippGetCount(supported);

    if(is_present_input_auto_exposure)
    {
      inner_attr = ippFindAttribute(coll, "input-auto-exposure", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-auto-exposure")) || ippGetValueTag(inner_attr) != IPP_TAG_BOOLEAN || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }           
    }

    if(is_present_input_auto_scaling)
    {
      inner_attr = ippFindAttribute(coll, "input-auto-scaling", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-auto-scaling")) || ippGetValueTag(inner_attr) != IPP_TAG_BOOLEAN || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }        
    }

    if(is_present_input_auto_skew_correction)
    {
      inner_attr = ippFindAttribute(coll, "input-auto-skew-correction", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-auto-skew-correction")) || ippGetValueTag(inner_attr) != IPP_TAG_BOOLEAN || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }     
    }

    if(is_present_input_brightness)
    {
      inner_attr = ippFindAttribute(coll, "input-brightness", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-brightness")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }       
    }

    // TODO: if color-mode is not provided and there is no color-mode-default set
    if(is_present_input_color_mode)
    {
      inner_attr = ippFindAttribute(coll, "input-color-mode", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-color-mode")) || ippGetValueTag(inner_attr) != IPP_TAG_KEYWORD || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }
      else
      {
        inner_supported = ippFindAttribute(client->printer->driver_attrs, "input-color-mode-supported", IPP_TAG_ZERO);
        attr_string_val = ippGetString(inner_attr,0,NULL);
        if (!ippContainsString(inner_supported, attr_string_val))
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }
      }             
    }

    if(is_present_input_content_type)
    {
      inner_attr = ippFindAttribute(coll, "input-content-type", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-content-type")) || ippGetValueTag(inner_attr) != IPP_TAG_KEYWORD || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }      
    }

    if(is_present_input_contrast)
    {
      inner_attr = ippFindAttribute(coll, "input-contrast", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-contrast")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }     
    }

    if(is_present_input_film_scan_mode)
    {
      inner_attr = ippFindAttribute(coll, "input-film-scan-mode", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-film-scan-mode")) || ippGetValueTag(inner_attr) != IPP_TAG_KEYWORD || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }      
    }

    if(is_present_input_images_to_transfer)
    {
      inner_attr = ippFindAttribute(coll, "input-images-to-transfer", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-images-to-transfer")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }      
    }

    if(is_present_input_orientation_requested)
    {
      inner_attr = ippFindAttribute(coll, "input-orientation-requested", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-orientation-requested")) || ippGetValueTag(inner_attr) != IPP_TAG_KEYWORD || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }
      else
      {
        inner_supported = ippFindAttribute(client->printer->attrs, "input-orientation-requested-supported", IPP_TAG_ZERO);
        attr_string_val = ippGetString(inner_attr,0,NULL);
        if (!ippContainsString(inner_supported, attr_string_val))
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }
      }     
    }

    if(is_present_input_media)
    {
      inner_attr = ippFindAttribute(coll, "input-media", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-media")) || ippGetValueTag(inner_attr) != IPP_TAG_KEYWORD || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }
      else
      {
        inner_supported = ippFindAttribute(client->printer->driver_attrs, "input-media-supported", IPP_TAG_ZERO);
        attr_string_val = ippGetString(inner_attr,0,NULL);
        if (!ippContainsString(inner_supported, attr_string_val))
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }
      }       
    }

    if(is_present_input_quality)
    {
      inner_attr = ippFindAttribute(coll, "input-quality", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-quality")) || ippGetValueTag(inner_attr) != IPP_TAG_ENUM || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }
      else
      {
        inner_supported = ippFindAttribute(client->printer->attrs, "input-quality-supported", IPP_TAG_ZERO);
        attr_string_val = ippGetString(inner_attr,0,NULL);
        if (!ippContainsString(inner_supported, attr_string_val))
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }
      }       
    }

    if(is_present_input_resolution)
    {
      inner_attr = ippFindAttribute(coll, "input-resolution", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-resolution")) || ippGetValueTag(inner_attr) != IPP_TAG_RESOLUTION || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }
      else
      {
        int y_res_support,  // vertical resolution supported (iterate) 
          x_res_support,    // horizontal resolution supported (iterate) 
          y_res,    // vertical resolution input 
          x_res;    // horizontal resolution input 

        flag = 0;
        inner_supported = ippFindAttribute(client->printer->driver_attrs, "input-resolution-supported", IPP_TAG_ZERO);
        num_supported = ippGetCount(inner_supported);

        x_res = ippGetResolution(inner_attr, 0, &y_res, IPP_RES_PER_INCH);

        for(i = 0; i < num_supported; i++)
        {
          x_res_support = ippGetResolution(inner_supported, i, &y_res_support, IPP_RES_PER_INCH);
          if (x_res == x_res_support && y_res == y_res_support)
          {
            flag = 1;
            break;
          }
          if (!flag)
          {
            respond_unsupported(client, inner_attr);
            valid = 0;  
          }
        }
      }      
    }

    if(is_present_input_scaling_height)
    {
      inner_attr = ippFindAttribute(coll, "input-scaling-height", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-scaling-height")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }    
    }

    if(is_present_input_scaling_width)
    {
      inner_attr = ippFindAttribute(coll, "input-scaling-width", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-scaling-width")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }      
    }

    if(is_present_input_scan_regions)
    {
      inner_attr = ippFindAttribute(coll, "input-scan-regions", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-scan-regions")) || ippGetValueTag(inner_attr) != IPP_TAG_BEGIN_COLLECTION || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }      
    }

    if(is_present_input_sharpness)
    {
      inner_attr = ippFindAttribute(coll, "input-sharpness", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-sharpness")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }      
    }

    if(is_present_input_sides)
    {
      inner_attr = ippFindAttribute(coll, "input-sides", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-sides")) || ippGetValueTag(inner_attr) != IPP_TAG_KEYWORD || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }
      else
      {
        inner_supported = ippFindAttribute(client->printer->driver_attrs, "input-sides-supported", IPP_TAG_ZERO);
        attr_string_val = ippGetString(inner_attr,0,NULL);
        if (!ippContainsString(inner_supported, attr_string_val))
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }
      }       
    }

    if(is_present_input_source)
    {
      inner_attr = ippFindAttribute(coll, "input-source", IPP_TAG_ZERO);
      if(!(ippContainsString(supported,"input-source")) || ippGetValueTag(inner_attr) != IPP_TAG_KEYWORD || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
      {
        respond_unsupported(client, inner_attr);
        valid = 0; 
      }
      else
      {
        inner_supported = ippFindAttribute(client->printer->driver_attrs, "input-source-supported", IPP_TAG_ZERO);
        attr_string_val = ippGetString(inner_attr,0,NULL);
        if (!ippContainsString(inner_supported, attr_string_val))
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }
      }       
    }
  }
  else
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "input-attributes is a required attribute");
    valid = 0;
  }

  // Check if the supplied output attributes are supported
  // Note that output-attribute member attributes do not have a corresponding "-supplied" attribute defined.
  //  they will therefore be processed during job-processing

  if ((attr = ippFindAttribute(client->request, "output-attributes", IPP_TAG_ZERO)) != NULL)
    {
      coll = ippGetCollection(attr,0);
      bool    is_present_noise_removal = (ippFindAttribute(coll, "noise-removal", IPP_TAG_ZERO) != NULL),
        is_present_output_compression_quality_factor = (ippFindAttribute(coll, "output-compression-quality-factor", IPP_TAG_ZERO) != NULL),

      supported = ippFindAttribute(client->printer->driver_attrs, "output-attributes-supported", IPP_TAG_ZERO);
      num_supported = ippGetCount(supported);

      if(is_present_noise_removal)
      {
        inner_attr = ippFindAttribute(coll, "noise-removal", IPP_TAG_ZERO);
        if(!(ippContainsString(supported,"noise-removal")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }           
      }

      if(is_present_output_compression_quality_factor)
      {
        inner_attr = ippFindAttribute(coll, "output-compression-quality-factor", IPP_TAG_ZERO);
        if(!(ippContainsString(supported,"output-compression-quality-factor")) || ippGetValueTag(inner_attr) != IPP_TAG_INTEGER || ippGetGroupTag(inner_attr) != IPP_TAG_OPERATION)
        {
          respond_unsupported(client, inner_attr);
          valid = 0; 
        }        
      }
    }

  return (valid);
}


//
// 'valid_scan_job_attributes()' - Determine whether the scan job attributes are valid.
//
// When one or more job attributes are invalid, this function adds a suitable
// response and attributes to the unsupported group.
//

static int				// O - 1 if valid, 0 if not
valid_scan_job_attributes(
    pappl_client_t *client)		// I - Client
{
  int			i,		// Looping var
			count,		// Number of values
			valid = 1;	// Valid attributes?
  ipp_attribute_t	*attr,		// Current attribute
      *inner_attr, // member attribute
			*supported;	// xxx-supported attribute
  ipp_t *coll;    // current collection


  // If a shutdown is pending, do not accept more jobs...
  if (client->system->shutdown_time)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return (0);
  }

  // Check operation attributes...
  valid = valid_doc_attributes(client);

  pthread_rwlock_rdlock(&client->printer->rwlock);

  // Check the various job template attributes...
  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) != 1)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "no-hold"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || (strcmp(ippGetString(attr, 0, NULL), "separate-documents-uncollated-copies") && strcmp(ippGetString(attr, 0, NULL), "separate-documents-collated-copies")))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges", IPP_TAG_ZERO)) != NULL)
  {
    int upper = 0, lower = ippGetRange(attr, 0, &upper);
					// "page-ranges" value

    if (!ippGetBoolean(ippFindAttribute(client->printer->attrs, "page-ranges-supported", IPP_TAG_BOOLEAN), 0) || ippGetValueTag(attr) != IPP_TAG_RANGE || ippGetCount(attr) != 1 || lower < 1 || upper < lower)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "number-of-retries", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
      {
        respond_unsupported(client, attr);
        valid = 0;
      }
  }

  if ((attr = ippFindAttribute(client->request, "retry-interval", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1)
      {
        respond_unsupported(client, attr);
        valid = 0;
      }
  }

  if ((attr = ippFindAttribute(client->request, "retry-timeout", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1)
      {
        respond_unsupported(client, attr);
        valid = 0;
      }
  }

  if ((attr = ippFindAttribute(client->request, "destination-uris", IPP_TAG_ZERO)) != NULL)
  {
    char	scheme[32],		// URI scheme
      userpass[32],		// Username/password in URI
      host[256],		// Host name in URI
      resource[256],		// Resource path in URI
      *resptr;		// Pointer into resource
      int	port,			// Port number in URI
      job_id;			// Job ID

    coll = ippGetCollection(attr,0);
    bool    is_present_destination_attributes = (ippFindAttribute(coll, "destination-attributes", IPP_TAG_ZERO) != NULL),
      is_present_destination_uri = (ippFindAttribute(coll, "destination-uri", IPP_TAG_ZERO) != NULL),
      is_present_post_dial_string = (ippFindAttribute(coll, "post-dial-string", IPP_TAG_ZERO) != NULL),
      is_present_pre_dial_string = (ippFindAttribute(coll, "pre-dial-string", IPP_TAG_ZERO) != NULL),
      is_present_t33_subaddress = (ippFindAttribute(coll, "t33-subaddress", IPP_TAG_ZERO) != NULL);

    // t33-subaddress | pre-dial-string | post-dial-string must not be present
    if (is_present_post_dial_string || is_present_pre_dial_string || is_present_t33_subaddress)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "destination-uris called with t33-subaddress|pre-dial-string|post-dial-string");
      respond_unsupported(client, attr);
      valid = 0;      
    }

    if (!is_present_destination_uri)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "destination-uris called without URI");
      respond_unsupported(client, attr);
      valid = 0; 
    }

    // process uri
    inner_attr = ippFindAttribute(coll, "destination-uri", IPP_TAG_ZERO);
    if (httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(inner_attr, 0, NULL), scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad URI value '%s'.", ippGetString(inner_attr, 0, NULL));
      respond_unsupported(client, inner_attr);
      valid = 0; 
    }
    if (!strcmp(scheme,"tel") || !strcmp(scheme,"fax") || !strcmp(scheme,"sip") || !strcmp(scheme,"sips"))
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad URI Scheme '%s'. tel,fax,sip,sips are not supported", scheme);
      respond_unsupported(client, inner_attr);
      valid = 0; 
    }

  } 

  pthread_rwlock_unlock(&client->printer->rwlock);

  return (valid);
}
