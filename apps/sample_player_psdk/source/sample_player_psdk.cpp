#include <iostream>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

#include "psdk/PSDK.h"
#include "psdk/MediaPlayerEvents.h"
#include "psdk/MediaPlayerItemConfig.h"
#include "DefaultCallbackManager.h"

#include <gst/gst.h>

using namespace std;
using namespace psdk;
void * gMainWindow;
guint cccounter =0;
PSDKSharedPointer<MediaPlayerItemConfig> config;//new BlackOutConfig(dispatcher);

static void handle_cmd(guint cmd);
void LogI(const char *format, ... )
{
   FILE* pFile;
   char final[512];
   char buffer[256];
   char xbuffer[30];
   struct timeval tv;
   time_t curtime;
   memset(xbuffer,0x00,30);
   memset(buffer,0x00,256);
   memset(final,0x00,512);
   gettimeofday(&tv, NULL);
   curtime=tv.tv_sec;
   strftime(xbuffer,29,"%m-%d-%Y  %T.",localtime(&curtime));
   va_list args;
   va_start (args, format);
   vsnprintf(buffer, 255, format, args);
   va_end (args);
   snprintf(final,511,"%s%ld %s %s\n",xbuffer,tv.tv_usec, "test", buffer);
   int strl = strlen(final);
   pFile = fopen("x.log", "a");
   if(pFile)
   {
      fwrite (final, sizeof(char), strl, pFile);
      fclose(pFile);
   }
   return;
}


PSDK*  psdk1;
Surface gSurface;

class ViewImpl : public psdk::View
{
   private:
      Surface _surface;
      SurfaceType _type;
      int32_t xcoord;
      int32_t ycoord;
      int32_t width;
      int32_t height;
   public: 
      ViewImpl(Surface surface, SurfaceType type)
         : _surface(surface)
           , _type(type)
           , xcoord(0)
           , ycoord(0)
           , width(300)
           , height(150)
           ,_refCount(0)
   {}

      virtual int32_t getX() { return xcoord; }
      virtual int32_t getY() { return ycoord; }
      virtual int32_t getWidth() { return width; }
      virtual int32_t getHeight() { return height; }
      virtual PSDKErrorCode setPos(int32_t x, int32_t y) 
      { 
         return kECSuccess; 
      }
      virtual PSDKErrorCode setSize(int32_t width, int32_t height) 
      {
         this->width = width;
         this->height = height;
         return kECSuccess; 
      }

      virtual Surface getNativeSurface() { return _surface;}
      virtual SurfaceType getNativeSurfaceType() { return _type;}

      CANHAVEUSERDATAINTERFACE_METHODS(View)
         GETTOPLEVELIID_METHOD(View)
};

void onProgress(const PSDKEvent *evt, uintptr_t userdata)
{
   PSDKSharedPointer<PSDKEventTargetInterface> iface;
   PSDKSharedPointer<MediaPlayer> player;
   const PSDKSharedPointer<const PSDKEvent> e(evt);
   if (e->getTarget(iface.capture()) != kECSuccess)
      return;
   player = iface.interfaceCast<MediaPlayer>(IIDOF(MediaPlayer));
   LogI("POSTION  onProgress currentTime = %ld", (long)player->getCurrentTime());
}

void onPrepared(const PSDKEvent *evt, uintptr_t userdata)
{

   LogI("STATE onPrepared RECIEVED ");
   PSDKSharedPointer<PSDKEventTargetInterface> iface;
   PSDKSharedPointer<MediaPlayer> player;
   const PSDKSharedPointer<const PSDKEvent> e(evt);
   if (e->getTarget(iface.capture()) != kECSuccess)
   {
      LogI("Returning...");
      return;
   }

   player = iface.interfaceCast<MediaPlayer>(IIDOF(MediaPlayer));
   LogI(" RDKAV %s:%d CALLING PLAYER PLAY",__FUNCTION__,__LINE__);
   player->play();
   LogI(" RDKAV %s:%d DONE PLAYER PLAY",__FUNCTION__,__LINE__);
}

void onTimedEvent(const PSDKEvent *evt, uintptr_t userdata)
{

   const PSDKSharedPointer<const PSDKEvent> e(evt);
   PSDKSharedPointer<TimedEvent> te = e.interfaceCast<TimedEvent>(IIDOF(TimedEvent));
   if(te)
   {
      LogI("\nonTimedEvent  name %s description %s \n ", te->getName().Data(), te->getDescription().Data());
   }

}

void onStateChanged(const PSDKEvent *evt, uintptr_t userdata)
{
   //LogI("\nonStateChanged \n\n");
   PSDKSharedPointer<PSDKEventTargetInterface> iface;
   PSDKSharedPointer<MediaPlayer> player;
   const PSDKSharedPointer<const PSDKEvent> e(evt);
   if (e->getTarget(iface.capture()) != kECSuccess)
      return;
   player = iface.interfaceCast<MediaPlayer>(IIDOF(MediaPlayer));

   PSDKSharedPointer<MediaPlayerStatusChangedEvent> s;
   s = e.interfaceCast<MediaPlayerStatusChangedEvent>(IIDOF(MediaPlayerStatusChangedEvent));

   if(s->getStatus() == kPSPrepared)
   {
      LogI(" RDKAV %s:%d PREPARED STATE RECIEVED ",__FUNCTION__,__LINE__);
      onPrepared(evt, userdata);
   }
   if(s->getStatus() == kPSReleased)
   {
      LogI(" RDKAV %s:%d RELEASED STATE RECIEVED ",__FUNCTION__,__LINE__);
      PSDK *psdk1;
      PSDK::getPSDK(psdk1);
      psdk1->release();
   }

   LogI("ON STATE CHANGED State: %d", s->getStatus());
   PSDKSharedPointer<const Metadata> metadata;
   s->getMetadata(metadata.capture());
   if(metadata)
   {
      PSDKSharedPointer<PSDKImmutableValueArray<PSDKString> > keys;
      metadata->getKeySet(&keys);
      for(uint32_t i = 0; i < keys->getSize(); i++)
      {
         LogI("keys[%s] = %s\n", keys->getAt(i).Data(), metadata->getValue(keys->getAt(i)).Data());
      }
   }

   if (s->getStatus() == kPSInitialized)
   {
      LogI(" RDKAV %s:%d INITIALIZED PREPARE TO PLAY ",__FUNCTION__,__LINE__);
      player->prepareToPlay();
      LogI(" RDKAV %s:%d INITIALIZED PREPARE TO PLAY DONE ",__FUNCTION__,__LINE__);
   }
}


void onBitrateChanged(const PSDKEvent *evt, uintptr_t userdata)
{
   PSDKSharedPointer<PSDKEventTargetInterface> iface;
   PSDKSharedPointer<MediaPlayer> player;
   const PSDKSharedPointer<const PSDKEvent> e(evt);
   if (e->getTarget(iface.capture()) != kECSuccess)
      return;
   player = iface.interfaceCast<MediaPlayer>(IIDOF(MediaPlayer));
}

PSDKSharedPointer<MediaPlayer> gPlayer;

static gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, void *data) 
{
   gchar *str = NULL;

   LogI("\n------------------------HANDLE JEYBOARD ------------------------\n");
   if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL) {
      return TRUE;
   }
   handle_cmd(g_ascii_tolower (str[0]));
   g_free (str);
}

static void handle_cmd(guint cmd)
{

   PSDKSharedPointer<MediaPlayer> player = gPlayer;
   PSDKErrorCode  error;
   LogI("\n------------------------HANDLE CMD %d pthread %d------------------------\n",cmd,pthread_self());
   switch(cmd)
   {
      case 32 : //space
         {
            printf("\n------------------------PAUSING VIDEO------------------------\n");
            LogI("------------------------PAUSING VIDEO------------------------");
            error = player->pause();
            LogI("Return Value for pausing: %d", error);
         }
         break;

      case 102: //f
         {
            LogI("------------------------ Fast forward VIDEO------------------------");
            static float rate = 1.0f;
            rate = rate * 2;
            MediaPlayerItem* mpItem;
            error = player->getCurrentItem(mpItem);
            PSDKSharedPointer<PSDKImmutableValueArray<float> > supportedRates;
            LogI("Start Supported Rates: ");
            if(kECSuccess == mpItem->getAvailablePlaybackRates(&supportedRates))
            {
               for(int count = 0; count <supportedRates->getSize(); count++ )
               {
                  float value = (*supportedRates)[count];
                  LogI("   %f", value);
               }
            }
            LogI("End Supported Rates: ");
            LogI("Current setRate:%f", rate);
            error = player->setRate(rate);
            LogI("Return Value for setRate: %d", error);
            break;
         }
      case 112: //p
         {
            LogI("------------------------ play VIDEO------------------------");
            error = player->play();
            LogI("Return Value for playing: %d", error);
            break;
         }
      case 120: //x
         {
            player->release();
            printf("\n\n ################## EXITING PLAYER #######################\n\n");

         }
         break;
      case 116: //x
         {
            double pos = player->getCurrentTime();
            printf(" ################## GET CURRENT TIME %f #######################\n",pos);
            LogI(" ################## GET CURRENT TIME %f #######################\n",pos);

         }
         break;
      case 114: //r
         {    	
            MediaResource res;
            LogI("Reset Player");
            PSDKErrorCode error = player->reset();
            LogI("Return value for player reset: %d", error);
            cccounter++;
            //const char* url3 ="http://www.streambox.fr/playlists/test_001/stream.m3u8";
            //const char* url2 ="http://www.overdigital.com/demos/ads/geico1/playlist.m3u8";
            const char * url3 ="http://amssamples.streaming.mediaservices.windows.net/91492735-c523-432b-ba01-faba6c2206a2/AzureMediaServicesPromo.ism/manifest(format=m3u8-aapl).m3u8";
            const char * url2 = "http://cbsnewshd-lh.akamaihd.net:80/i/CBSNHD_7@199302/index_1200_av-p.m3u8";
            char * url = NULL;
            if(cccounter % 2 == 0)
               url = url3;
            else
               url = url2;

            res = MediaResource(PSDKString(url), kMRTHLS, NULL);
            error =player->replaceCurrentResource(res, config);
            LogI("Return value for player replaceResource: %d", error);

         }
         break;
      case 121: //y
         {
            player->setCCVisibility(MediaPlayer::kVisible);
         }
         break;
      case 122: //z
         {
            printf("Seeking....");
            LogI("Seeking....");
            LogI(" ################## SEEKING sTART  #######################\n");
            LogI(" ################## SEEKING 5000  #######################\n");
            //player->seek(20000);
            error = player->seek( 10000 );
            LogI("Return Value for seeking: %d", error);
            LogI(" ################## SEEKING DONE #######################\n");
            break;
         }
   }

   return true;
}

bool EndsWith(const char* str, const char* p)
{
   size_t len = strlen(str);
   size_t patternLen = strlen(p);

   return strcmp(str + len - patternLen, p) == 0;
}

void exitFunction()
{
   LogI("exitFunction called\n");
   kernel::IKernel::UninitializeKernel();
}

gboolean file_stat(gpointer data)
{

#define CMDFILE "./cmdfile"
   FILE *cmdfile;
   struct stat fileStat;
   if(stat(CMDFILE,&fileStat)==0)
   {
      LogI("\nAPP::FILE available \n");
      cmdfile= fopen(CMDFILE, "r");
      int command= 0;
      int ret = fscanf(cmdfile, "%d", &command);
      int fe = feof(cmdfile);
      LogI("\nAPP::COMMAND %d READ  fe %d ret %d \n",command,fe,ret);
      fclose(cmdfile);
      remove(CMDFILE);
      handle_cmd(command);
   }
   return TRUE;
}


int main (int argc, char* argv[])
{
   if (PSDK::getPSDK(psdk1) != kECSuccess)
      return -1;

   atexit(exitFunction);

   //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   const char* url = NULL;
   const char* ad = NULL;
   MediaResourceType type = kMRTHLS;
   if (argc < 2)
   {
      LogI("no url specified \n" );
      return 0;
   }
   else
   {
      url = argv[1];
      ad = argv[2];

      if (EndsWith(url, "m3u8") || EndsWith(url, "M3U8"))
      {
         type = kMRTHLS;
      }
      else if(EndsWith(url, "mpd"))
      {
         type = kMRTDASH;
      }
      else
      {
         LogI("Media container type couldn't be figured out by looking at the url, assuming kMRTypeHLS\n");
      }
   }

   //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   GMainLoop *loop;
   GIOChannel *io_stdin;
   loop = g_main_loop_new ( NULL , FALSE );
   io_stdin = g_io_channel_unix_new (fileno (stdin));
   //g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, NULL);

   PSDKSharedPointer<PSDKEventManager> cbManager = new DefaultCallbackManager();

   PSDKSharedPointer<PSDKEventDispatcher> dispatcher;
   //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   psdk1->createDispatcher(cbManager, &dispatcher);

   //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   PSDKSharedPointer<MediaPlayer> player;
   if (psdk1->createMediaPlayer(dispatcher, NULL, &player) != kECSuccess)
      return -1;

   PSDKSharedPointer<Metadata> metadata;
   //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);

   psdk1->createMetadata(&metadata);



   //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   MediaResource res(PSDKString(url), type, metadata);


   //LogI("**********get PSDKVersion %s\n", Version::getVersion().Data());


   //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   dispatcher->addEventListener(kEventTimeChanged, onProgress, NULL);
   dispatcher->addEventListener(kEventProfileChanged, onBitrateChanged, NULL);
   dispatcher->addEventListener(kEventStatusChanged, onStateChanged, NULL);
   dispatcher->addEventListener(kEventTimedEvent, onTimedEvent, NULL);
   PSDKSharedPointer<View> view = PSDKSharedPointer<View>(new ViewImpl(gSurface,kSTGTKWidget));
   player->setView(view);

   {
      //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
      psdk1->createDefaultMediaPlayerItemConfig(&config);
      PSDKSharedPointer<AuditudeSettings> auditudeSettings;

      //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
      if(ad && (strcmp(ad ,"y")==0))
      {
#if 1
         psdk1->createAuditudeSettings(&auditudeSettings);
         auditudeSettings->setDomain("auditude.com"); 
         auditudeSettings->setMediaId("adbe_tearsofsteel2");
         auditudeSettings->setZoneId("123869"); 
         config->setAdvertisingMetadata(auditudeSettings);
#endif
      }

      LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
      player->replaceCurrentResource(res, config);
      LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   }

   gPlayer = player;
   BufferControlParameters bparam(1000, 100000);
   PSDKErrorCode error = player->setBufferControlParameters(bparam);
   LogI("setBufferParamCode:%d", error);

   g_timeout_add (500, file_stat, NULL);

   LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
   g_main_loop_run (loop);
   g_main_loop_unref(loop);
   LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);

   return 0;
}

void gstCheckerfn(void)
{
#if 0
   GstElement *pipeline;
   GstBus *bus;
   GstMessage *msg;

   /* Initialize GStreamer */
   gst_init (&argc, &argv);

   /* Build the pipeline */
   pipeline = gst_parse_launch ("playbin uri=http://localhost:80/test.mpeg video-sink=ximagesink", NULL);
   /* Start playing */
   gst_element_set_state (pipeline, GST_STATE_PLAYING);

   /* Wait until error or EOS */
   bus = gst_element_get_bus (pipeline);
   msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

   /* Free resources */
   if (msg != NULL)
      gst_message_unref (msg);
   gst_object_unref (bus);
   gst_element_set_state (pipeline, GST_STATE_NULL);
   gst_object_unref (pipeline);
#endif
}


