#include <gst/gst.h>
#include <glib.h>

/*
For getopt
*/
#include <unistd.h>


typedef enum {
  INVALID_TEST = 0,
  STANDBY_TO_FIRST_SHOT,
  SHOT_TO_SHOT,
  SHOT_TO_SAVE
  } ShotTestCases;

/*
Global variables
*/
static GstElement *pipeline;
static GHashTable *timestamps;
ShotTestCases option=INVALID_TEST;


/*
Functions
*/

static void
print_usage(char* appname)
{
	 g_printerr("Usage: %s [-o 0=STANDBY_TO_FIRST_SHOT | 1=SHOT_TO_SHOT | 2=SHOT_TO_SAVE] [-p pipeline] [-l log file]\n",appname);
	 g_printerr("\nPipelines Examples:\n\n");
	 g_printerr("\t %s -o 0 -l out.txt -p \"goocamera num-buffers=1 imagecap=true display-pos-x=50 display-pos-y=200 ! video/x-raw-yuv, format=(fourcc)UYVY, width=640, height=480, framerate=10/1 ! fakesink\"\n\n", appname);
	 g_printerr("\t %s -o 1 -l out.txt -p \"goocamera num-buffers=1 imagecap=true display-pos-x=50 display-pos-y=200 ! video/x-raw-yuv, format=(fourcc)UYVY, width=640, height=480, framerate=10/1 ! fakesink\"\n\n", appname);
	 g_printerr("\t %s -o 2 -l out.txt -p \"goocamera num-buffers=1 imagecap=true display-pos-x=50 display-pos-y=200 ! video/x-raw-yuv, format=(fourcc)UYVY, width=640, height=480, framerate=10/1 ! goofilter_vpp ! gooenc_jpeg ! filesink location=test.jpeg\"\n\n", appname);
}

/*
Get input arguments
*/
static gboolean
get_input_arguments(
	int argc,
	char **argv,
	ShotTestCases *option,
	gchar **output_result_file,
	gchar** str_pipeline)

{


    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "o:p:l:")) != -1)
    {
     switch (c)
       {
       case 'l':
         *output_result_file = optarg;
         break;
       case 'p':
         *str_pipeline = optarg;
         break;
       case 'o':
         {
         switch (atoi (optarg))
           {
           case 0:
             *option = STANDBY_TO_FIRST_SHOT;
             break;
           case 1:
             *option = SHOT_TO_SHOT;
             break;
           case 2:
             *option = SHOT_TO_SAVE;
             break;
           default:
             *option = INVALID_TEST;
             break;
           }
           break;
         }
       case '?':
         {
           g_printerr ("Invalid arguments\n");
             print_usage (argv[0]);
             return FALSE;
         }
       default:
         abort ();
       }
    }

    /*
        necessary parameters
    */
    if (*option == INVALID_TEST)
    {
        g_printerr ("Invalid or missing argument [-o option]\n");
        print_usage (argv[0]);
        return FALSE;
    }

    if (!str_pipeline)
    {
        g_printerr ("Missing argument [-p option]\n");
        print_usage (argv[0]);
        return FALSE;
    }

    return TRUE;
}

static gchar *
get_timestamp_string(GstStateChange transition)
{
  return g_strdup_printf("camera_transition_%d",transition);
}

static gdouble
get_state_transition_timestamp_double(GstStateChange transition)
{
  gchar *str;
  gdouble timestamp;
  gpointer value;

  str = get_timestamp_string(transition);
  value = (gchar *)g_hash_table_lookup(timestamps,str);
  g_return_val_if_fail(value != NULL, 0);
  timestamp = atof(value);
  if (str)
    g_free (str);
  return timestamp;
}

static gdouble
get_timestamp_double (gchar *str)
{
  gdouble timestamp;
  gpointer value;

  value = (gchar *)g_hash_table_lookup(timestamps,str);
  g_return_val_if_fail(value != NULL, 0);
  timestamp = atof(value);
  return timestamp;
}



static gint
print_results(ShotTestCases option, gchar *output_result_file)
{
    gdouble t0 = 0;
    gdouble t1 = 0;
    gdouble delta = 0;
    gchar *str_test_name = NULL;

    switch(option)
    {
        case STANDBY_TO_FIRST_SHOT:
        {
		t0 = get_state_transition_timestamp_double(GST_STATE_CHANGE_NULL_TO_READY);
		t1 = get_state_transition_timestamp_double(GST_STATE_CHANGE_PAUSED_TO_PLAYING);
		str_test_name = g_strdup_printf("standby-to-first-shot");
		break;
        }
        case SHOT_TO_SHOT:
	{
		t0 = get_state_transition_timestamp_double(GST_STATE_CHANGE_PAUSED_TO_PLAYING);
		t1 = get_state_transition_timestamp_double(GST_STATE_CHANGE_PLAYING_TO_PAUSED);
		str_test_name = g_strdup_printf("shot-to-shot");
		break;
        }
        case SHOT_TO_SAVE:
	{
		gpointer value;
		t0 = get_state_transition_timestamp_double(GST_STATE_CHANGE_PAUSED_TO_PLAYING);
		t1 = get_timestamp_double("shot-to-save-endpoint");
	        str_test_name = g_strdup_printf("shot-to-save");
		break;
        }
        default:
          break;
    }

    g_return_val_if_fail(t0 > 0, -1);
    g_return_val_if_fail(t1 > 0, -1);
    g_return_val_if_fail(t1 > t0, -1);
    delta = t1 - t0;

    FILE *file_id;
    file_id = fopen(output_result_file,"a+");

    g_print("%s\t%f\n",
        str_test_name,delta);

    g_fprintf(file_id, "%s\t%f\n",
        str_test_name,delta);


    g_print("Result appended in file %s\n", output_result_file);

    fclose(file_id);
    if (str_test_name)
        g_free (str_test_name);

    return 0;
}
  
static gboolean
endpoint_reached ()
{
	gchar *str = NULL;
	gboolean retval = FALSE;

	if (option == SHOT_TO_SAVE)
	  str = g_strdup_printf("%s","shot-to-save-endpoint");
	else
	  str = g_strdup_printf("camera_transition_%d",GST_STATE_CHANGE_PLAYING_TO_PAUSED);

	retval = g_hash_table_lookup(timestamps, str) != NULL;

	if (str)
		g_free (str);

	return retval;
}


static gboolean
message_comes_from_component(gchar *component_name, GstMessage *msg)
{
  return g_str_has_prefix(gst_object_get_name(GST_MESSAGE_SRC(msg)),component_name);
}

static gboolean
message_has_structure(GstMessage *msg)
{
  return (gst_message_get_structure(msg) != NULL);
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) 
  {

    case GST_MESSAGE_EOS:
    {
      gst_element_set_state (pipeline, GST_STATE_PAUSED);
      break;
    }

    case GST_MESSAGE_ERROR: 
    {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);
                 
      g_main_loop_quit (loop);
      break;
    }
    
    case GST_MESSAGE_ELEMENT: 
    {
        if(message_has_structure(msg))
        {
          const GstStructure *structure;
          const gchar *name;
          const gchar *timestamp;

          structure = gst_message_get_structure(msg);
          name = gst_structure_get_name(structure);
          timestamp = gst_structure_get_string(structure,"timestamp");

          g_printf ("name =%s timestamp=%s\n", name, timestamp);

          if (timestamps == NULL)
          { /* create the hash which will contain the timestamps */
              timestamps = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
          }

          { /*insert the camera_transition_xx/timestamp comming from the component */
            gpointer value;
            value = g_hash_table_lookup(timestamps,name);

            /* Just insert new keys, do not overwrite */
            if (value == NULL)
            {
              g_hash_table_insert(timestamps, g_strdup(name), g_strdup(timestamp));
            }
          }

          /* Check if we are done */
          if (endpoint_reached ())
          {
              g_main_loop_quit (loop);
          }
        }

        break;
    }
    
    default:
      break;
  }

  return TRUE;
}

static gint
run_pipeline(int argc, char *argv[], gchar *str_pipeline)
{
  GMainLoop *loop;
  GstBus *bus;
  GError *error;
  
  /* Initialisation */
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  
  pipeline = gst_parse_launch(str_pipeline, NULL);
  
    
  g_return_val_if_fail(pipeline != NULL,-1);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* Set the pipeline to "playing" state*/
  gst_element_set_state (pipeline, GST_STATE_PLAYING);


  /* Iterate */
  g_main_loop_run (loop);


  /* Out of the main loop, clean up nicely */

  gst_object_unref (GST_OBJECT (pipeline));
  
  return 0;
}


int
main (int   argc,
      char *argv[])
{

/*
    Default values
*/
    gchar *output_result_file ="shot.txt";
    gchar *str_pipeline = NULL;

    /* Get input arguments */
    if (get_input_arguments (argc, argv, &option, &output_result_file, &str_pipeline) == FALSE)
    {
        return -1;
    }

    {/* Play the pipeline */
        g_assert( run_pipeline(argc,argv,str_pipeline) == 0);
    }

    {/* print the results */
        print_results (option, output_result_file);
    }

    { /* Cleaning global variables */
        g_hash_table_destroy (timestamps);
    }


    return 0;
}
