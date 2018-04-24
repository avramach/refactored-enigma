#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include "RDKGstAVDecoder.h"

extern void LogI(const char *format, ... );
//#define BCM_SOC 
#define ENABLE_AUDIO_DECODE
#define ENABLE_VIDEO_DECODE
#define ADD_ADTS_HEADER
//#define ENABLE_PTS_STAMPING 

//5MB: for over 1 sec of jitter buffer for 20Mpbs stream
#define VIDEO_APPSRC_FIFO_SIZE 5*1024*1024 
#define AUDIO_APPSRC_FIFO_SIZE 1*1024*1024

#define GST_STATE_CHANGE_WAIT_SECS 5

using namespace media;
static void writefile( const char * fname ,char * buf, unsigned int buf_len)
{
   FILE* pFile;
   pFile = fopen(fname, "a");
   if(pFile)
   {
      size_t count = fwrite (buf , sizeof(unsigned char), buf_len, pFile);
      if(count != buf_len)
         printf("Writing PANIC\n");
      fclose(pFile);
   }
}
//*************************************************************************************************
// Register Hardware decoder!!
//*************************************************************************************************
REGISTER_AV_PLATFORM_COMPONENT(IVideoDecoder, RDKGstAVDecoder, HardwareAVDecoder)


RDKGstAVDecoder::RDKGstAVDecoder(MediaComponentFactory* factory)
{
   LogI(" RDKGSTAV CONSTRUCTOR %s::%d\n",__FUNCTION__,__LINE__);
   mH264Utils.m_pSink = this;
   mMainLoopThreadContext = g_main_context_new();
   mLoop= g_main_loop_new (mMainLoopThreadContext, FALSE);
   if (mLoop== NULL)
   {
      LogI("CRIITICAL Error creating a new Loop %s:%d\n",__FUNCTION__,__LINE__);
   }
   mMainLoopThread = g_thread_new("gstavmainloop", g_main_loop_run, (void*)mLoop);
   if (!mMainLoopThread)
   {
      LogI("CRIITICAL Error creating a new thread for gmainLoop %s:%d\n",__FUNCTION__,__LINE__);
   }
   while (FALSE == g_main_loop_is_running(mLoop))
   {
      LogI("%s:%d Wait to For Main Loop \n",__FUNCTION__,__LINE__);
      g_usleep(100);
   }
   InitializeGstPipeline();
   g_mutex_init(&mPipelineMutex);
   g_cond_init(&mStateCondition);
   LogI(" RDKGSTAV CONSTRUCTOR %s::%d DONE\n",__FUNCTION__,__LINE__);
}

RDKGstAVDecoder::~RDKGstAVDecoder()
{
   LogI( "RDKGstAVDecoder DESTRUCTOR ");
   if (mPlaybin)
   {
      LogI( "AVRA START PIPELINE NULL");
      gst_element_set_state (mPlaybin, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT (mPlaybin));
      LogI( "AVRA COMPLETE PIPELINE NULL");
   }
   g_cond_clear(&mStateCondition);
   if (mBus)
   {
      gst_object_unref( GST_OBJECT(mBus) );
   }
   if (g_main_loop_is_running(mLoop))
   {
      if ( NULL != mBusWatch)
      {
         LogI("%s:%d Removing Bus Watch\n",__FUNCTION__,__LINE__);
         g_source_destroy( mBusWatch);
         g_source_unref( mBusWatch);
      }

      if (mLoop) 
      {
         LogI("%s:%d about to quit the main loop\n",__FUNCTION__,__LINE__);
         g_main_loop_quit (mLoop);
         g_main_loop_unref(mLoop);
      }
   }
   if (mMainLoopThread)
   {
      LogI("%s:%d Wait to join SIGNAL THREAD \n",__FUNCTION__,__LINE__);
      g_thread_join (mMainLoopThread);
      LogI("%s:%d COmpleted o join SIGNAL THREAD \n",__FUNCTION__,__LINE__);
   }
   if(NULL != mMainLoopThreadContext)
   {
      g_main_context_unref(mMainLoopThreadContext);
   }
   LogI( "RDKGstAVDecoder DESTRUCTOR COMPLETE");
}

bool RDKGstAVDecoder::GetCapabilities(DecoderObjectFactory* pFactory, Capabilities& capabilitiesToSet)
{
   LogI(" RDKGSTAV CAPABILITIES %s::%d\n",__FUNCTION__,__LINE__);
   capabilitiesToSet.audioCodecList[0] = kACAAC;
   capabilitiesToSet.audioCodecList[1] = kACAC3;
   capabilitiesToSet.audioCodecList[2] = kACEAC3;
   capabilitiesToSet.audioCodecList[3] = kACDTS;
   capabilitiesToSet.audioCodecList[4] = kACNoAudio;
   capabilitiesToSet.videoCodecList[0] = kVCH264;
   capabilitiesToSet.supportedVideoModes = kExternalPlayback;
   capabilitiesToSet.supportedVideoModesWithLowLatency = kExternalPlayback;
   capabilitiesToSet.numAudioCodecsSupported = 5;
   capabilitiesToSet.numVideoCodecsSupported = 1;
   capabilitiesToSet.isSlowMoSupported = true;
   LogI(" RDKGSTAV CAPABILITIES %s::%d\n",__FUNCTION__,__LINE__);
   return true;
}

bool RDKGstAVDecoder::InitializeDecompressor(const VideoMetaData& metaData, DecoderConfig& configToSet)
{

   LogI(" RDKGSTAV INIT DECOMPRESSOR %s::%d\n",__FUNCTION__,__LINE__);
   mStreamMetadata = metaData;
   mNotifier = metaData.notifier; //XXX: should a copy be taken ??

   configToSet.audioDecoderType = kDecoderTypeHardware;
   configToSet.videoDecoderType = kDecoderTypeHardware;
   configToSet.videoConfigFlags = 0;
   if (!ChangePipelineState(kStatePause))
   {
      LogI(" RDKGSTAV INIT DECOMPRESSOR FAILED TIMEOUT WAIT %s::%d %d\n",__FUNCTION__,__LINE__,mDecoderState);
      return FALSE;
   }
   SetupPreRollState();
   LogI(" RDKGSTAV GST PLAY CALL DONE %s::%d metaBufferTime %d\n",__FUNCTION__,__LINE__,metaData.bufferTime);
   return true;
}

void RDKGstAVDecoder::SetupPreRollState(void)
{
   /* We cannot transition to kStatePlaying without pushing any buffers for pipeline to
    * complete preroll */

   g_mutex_lock(&mPipelineMutex);
   mDecoderState = kStatePreroll;
   g_mutex_unlock(&mPipelineMutex);

   if (!ChangePipelineState(kStatePlay))
   {
      LogI(" RDKGSTAV SET PREROLL FAILED %s::%d %d\n",__FUNCTION__,__LINE__,mDecoderState);
      return FALSE;
   }
}

void RDKGstAVDecoder::GetQOSData(QOSData & qosDataToSet)
{
   return;
}

void RDKGstAVDecoder::SetBufferTime(int32_t bufferTime)
{
   LogI(" RDKGSTAV %s::%d BUFFERTIME %dms\n",__FUNCTION__,__LINE__,bufferTime);
}

void RDKGstAVDecoder::NotifyEOF()
{
   GstFlowReturn ret;
   LogI(" RDKGSTAV EOF NOTIFY START %s::%d\n",__FUNCTION__,__LINE__);
   mEosRecieved= true;
   if(mDynAppsrc)
   {
      g_signal_emit_by_name (mDynAppsrc, "end-of-stream", &ret);
   }
   LogI(" RDKGSTAV EOF NOTIFY COMPLTEE %s::%d\n",__FUNCTION__,__LINE__);

   return;
}

TimeStamp RDKGstAVDecoder::GetVideoPosition()
{
   gint64 position =0;
   TimeStamp decoderpts = TIME_UNKNOWN;

   if( mDecoderState != kStateStopped)
   {
      if (mStreamMetadata.videoCodec != kVCNoVideo ) 
      {
#if BCM_SOC_PTS
         if(mVideoDecoderElement)
         {  
            int64_t videoPts= 0ll;
            g_object_get(mVideoDecoderElement,"video_pts", &videoPts,NULL);
            LogI("RDKGST CURRENT Position timeForm %llu" GST_TIME_FORMAT " PTS=%llu PTS90=%llu",GST_TIME_ARGS (videoPts),videoPts,videoPts/90);
            decoderpts = videoPts;
         }
#else
         if (gst_element_query_position (mPlaybin, GST_FORMAT_TIME, &position)) 
         {
            LogI("RDKGST CURRENT Position %llu Time %" GST_TIME_FORMAT ,position,GST_TIME_ARGS (position));
            decoderpts = position;
         }
#endif
         else 
         {
            LogI(">> [WARN] %s -> GET VIDEO POSITION FAILED!!\n", __FUNCTION__);
         }
      }
      else 
      {
         LogI(">> [WARN] %s%d -> GET POSITION FAILED!!\n", __FUNCTION__,__LINE__);
      }
   }
   else 
   {
      //LogI(">> [WARN] %s%d -> GET POSITION FAILED ,WRONG STATE %d!!\n", __FUNCTION__,__LINE__,mDecoderState);
   }

   return decoderpts;
}

void RDKGstAVDecoder::Pause()
{
   LogI( "PAUSE pthread %d",pthread_self());
   LogI(" RDKGSTAV PAUSE INVOKED %s::%d mDecoderState %d\n",__FUNCTION__,__LINE__,mDecoderState);
   if(mDecoderState == kStatePlaying)
   {
      LogI( "START PIPELINE PAUSE");
      if (!ChangePipelineState(kStatePause))
      {
         LogI(" RDKGSTAV PAUSED FAILED TIMEOUT WAIT %s::%d %d\n",__FUNCTION__,__LINE__,mDecoderState);
      }
      LogI( "COMPLETE PIPELINE PAUSE");
   }
   return;
}

bool RDKGstAVDecoder::Resume()
{
   gboolean ret = TRUE;
   LogI(" RDKGSTAV RESUME INVOKED %s::%d mDecoderState %d\n",__FUNCTION__,__LINE__,mDecoderState);
   if((mDecoderState == kStatePaused) ||(mDecoderState == kStatePreroll ))
   {
      LogI( "START PIPELINE RESUMSE");
      if (!ChangePipelineState(kStatePlay))
      {
         LogI(" RDKGSTAV RESUME FAILED TIMEOUT WAIT %s::%d %d\n",__FUNCTION__,__LINE__,mDecoderState);
         ret = FALSE;
      }
      LogI( "COMPLETE PIPELINE RESUME");
   }
   return ret;
}

void RDKGstAVDecoder::Seek(TimeStamp position)
{
   LogI(" RDKGSTAV SEEK INVOKED %s::%d %llums mDecoderState %d \n",__FUNCTION__,__LINE__,position/1000000, mDecoderState);
   if( (mDecoderState == kStatePlaying) || (mDecoderState == kStatePaused) )
   {
      mSeekTimeStamp = position;
      FlushPipeline();
      g_mutex_lock(&mPipelineMutex);
      mDecoderState = kStatePreroll;
      int64_t level = gst_app_src_get_current_level_bytes (GST_APP_SRC (mVideoAppSrc));
      LogI(">>[INFO] %s : Buffer Status in Fifotype %d level %lu !!\n", __FUNCTION__,level);
      g_mutex_unlock(&mPipelineMutex);
      uint64_t pts_45Khz = (45/5) * (position/((1000 * 1000)/5));
      LogI("Seek to PTS45KHz:%llu PTS:%llu", pts_45Khz, position/1000000 );
   }
}

void RDKGstAVDecoder::FlushPipeline(void)
{
   gboolean sent = FALSE;
   LogI(" RDKGSTAV FLUSH %s::%d INVOKED \n",__FUNCTION__,__LINE__);

   GstEvent *flushStart = gst_event_new_flush_start();
   GstEvent *flushStop = gst_event_new_flush_stop(FALSE);

   g_mutex_lock(&mPipelineMutex);
   LogI(" RDKGSTAV FLUSH %s::%d START EVENT \n",__FUNCTION__,__LINE__);
   sent = gst_element_send_event(GST_ELEMENT(mDynAppsrc), flushStart);
   if(!sent)
   {
      LogI(" RDKGSTAV FLUSH %s::%d START EVENT SEND FAILED \n",__FUNCTION__,__LINE__);
   }
   LogI(" RDKGSTAV FLUSH %s::%d STOP EVENT \n",__FUNCTION__,__LINE__);
   sent = gst_element_send_event(GST_ELEMENT(mDynAppsrc), flushStop);
   if(!sent)
   {
      LogI(" RDKGSTAV FLUSH %s::%d STOP EVENT SEND FAILED \n",__FUNCTION__,__LINE__);
   }
   g_mutex_unlock(&mPipelineMutex);
   LogI(" RDKGSTAV FLUSH %s::%d COMPLETED \n",__FUNCTION__,__LINE__);
}


bool RDKGstAVDecoder::HandleStreamSwitch(const VideoMetaData & metadata)
{
   if(mStreamMetadata.audioCodec != metadata.audioCodec)
   {
      return false;
   }
   return true;
}

bool RDKGstAVDecoder::ConsumeData(StreamPayload* pPayload, bool interleaved)
{
   if( !interleaved )  return false;

   bool retval = false;

   if( pPayload->IsDroppable() && pPayload->PTS < mSeekTimeStamp)
   {
      LogI(">>[INFO] %s :Cannot consumeData PTS %llu  < SeekTimeStamp %llu !!\n", __FUNCTION__,pPayload->PTS, mSeekTimeStamp);
      pPayload->Release();
      return true;
   }
   if ( false == CanConsumeData(pPayload->payloadType, pPayload->length) )
   {
      LogI(">>[INFO] %s :Cannot consumeData Insufficient Buffer !!\n", __FUNCTION__);
      return false;
   }
#if 1
   if (kPTVideo == pPayload->payloadType ){
      LogI(">> ext [INFO] %s %s PTS=%llu PTSms=%llu PTS90=%llu        SZ:%d\n", __FUNCTION__,
            pPayload->payloadType == kPTVideo ? "video" :
            ( pPayload->payloadType == kPTAudio ? "audio" :
              ( pPayload->payloadType == kPTData ? "data" : "unknown" ) ),
            pPayload->PTS, pPayload->PTS/1000000, (pPayload->PTS)/90, pPayload->length ); 
   }
#endif

   if (kPTVideo == pPayload->payloadType )
   {
      mCurrentTimeStampVideo = pPayload->PTS;
      ConsumeVideoPayload(pPayload);
      retval = true;
   }
   else if (kPTAudio == pPayload->payloadType )
   {
      mCurrentTimeStampAudio= pPayload->PTS;
      ConsumeAudioPayload(pPayload);
      retval = true;
   }
   else
   {
      LogI(">>[ERR] %s : UnHandled payloadType", __FUNCTION__); 
      pPayload->Release();
      retval = true;
   }
   return retval;
};

bool RDKGstAVDecoder::CanConsumeData(PayloadType type, uint32_t length)
{
   gboolean ret = FALSE;
   if((mDecoderState  == kStatePlaying ) || (mDecoderState == kStatePreroll))
   {
      guint64 avail = 0;
      guint64 level = 0;
      guint64 totalsize = 0;
      GstElement * appsrc = NULL;
      PayloadType pType;

      if(type == kPTVideo)
      {
         appsrc = mVideoAppSrc;
         totalsize = VIDEO_APPSRC_FIFO_SIZE;
         pType = kPTVideo;
      }
      else if(type == kPTAudio)
      {
#if defined ENABLE_AUDIO_DECODE
         appsrc = mAudioAppSrc;
         totalsize = AUDIO_APPSRC_FIFO_SIZE;
         pType = kPTAudio;
#else
         return TRUE;
#endif
      }
      else
      {
         LogI(">>[ERR] %s : UnHandled payloadType", __FUNCTION__); 
         return ret;
      }

      level = gst_app_src_get_current_level_bytes (GST_APP_SRC (appsrc));
      avail = totalsize - level;

      if (level > totalsize * 0.80 )
      {
         mNotifier->NotifyBufferStatus(pType, kBufferHigh);
         LogI(">>[INFO] %s : Notify HighBuffer Status in Fifotype %d level %lu avail %lu total %lu!!\n", __FUNCTION__,type,level,avail,totalsize);
         ret = FALSE;
      }
      else if(length < avail)
      {
         ret = TRUE;
      }
      if(!ret)
      {
         LogI("<<%s CheckSpace=%d Type %d DecState %d InputSize %u FifoLevel %lu FifoSize %lu FifoRemain %lu[%lu %%]\n", __FUNCTION__, ret, type, mDecoderState, length, level, totalsize, avail, (avail*100)/totalsize);
      }
   }
   return ret;
}

bool RDKGstAVDecoder::ConsumeVideoPayload(StreamPayload* pPayload)
{
   bool result = true;

   switch (pPayload->frameType)
   {
      case StreamPayload::kFTH264_AVCC:
         {
            //LogI(">>[INFO] %s : Got AVCC packet [%u] [%u] --> %p !!\n", __FUNCTION__,
            //    (uint32_t)TimeStamp2MS(pPayload->PTS), pPayload->length, *((uint32_t*) pPayload->data));

            if(!mH264Utils.ParseAVCC(pPayload, true, false))
            {
               LogI(">>[INFO] %s : Parsing AVCC failed skipping the frame !!\n", __FUNCTION__);
               result = false;
            }
         }
         break;
      case StreamPayload::kFTKey:
      case StreamPayload::kFTP:
      case StreamPayload::kFTB:
         {
            //LogI(">>[INFO] %s : Got Video Payload %d [%u] [%u] --> %p !!\n", __FUNCTION__,
            //     pPayload->frameType, (uint32_t)TimeStamp2MS(pPayload->PTS), pPayload->length, *((uint32_t*) pPayload->data));
            if(!mH264Utils.ParseSample(pPayload, false))
            {
               LogI(">>[INFO] %s : Parsing H264 Sample failed skipping the frame !!\n", __FUNCTION__);
               result = false;
            }
         }
         break;
      default:
         {
            LogI(">>[WARN] %s : Unknown frame type %d Dropped!!\n", __FUNCTION__, pPayload->frameType);
         }
         break;
   }
   pPayload->Release();
   return result;
}

bool RDKGstAVDecoder::ConsumeAudioPayload(StreamPayload* pPayload)
{
   bool result = false;
   uint8_t adtsheader[RDKAACUtils::MAX_BUFFER_SIZE];
   uint32_t adtsheadersize = 0;
   switch (pPayload->frameType) 
   {
      case StreamPayload::kFTAAC_AudioSpec:
         {
            // LogI(">>[INFO] %s : Got StreamPayload::kFTAAC_AudioSpec [%u] [%u] --> %p !!\n", __FUNCTION__,
            //(uint32_t)TimeStamp2MS(pPayload->PTS), pPayload->length, *((uint32_t*) pPayload->data));
            mAacUtils.Reset();
            if ( !mAacUtils.Parse( pPayload->data, pPayload->length ) )
            {
               LogI(">>[WARN] %s : Parsing Audio Spec failed!!\n", __FUNCTION__); 
            }
            pPayload->Release();
            result = true;
         }
         break;
      case StreamPayload::kFTAudioSample:
         {
            //LogI(">>[INFO] %s : Got StreamPayload::kFTAudioSample [%u] [%u] --> %p !!\n", __FUNCTION__,
            //(uint32_t)TimeStamp2MS(pPayload->PTS), pPayload->length, *((uint32_t*) pPayload->data));
            if(mStreamMetadata.audioCodec ==  kACAAC && mAacUtils.IsValid())
            {
#if defined ADD_ADTS_HEADER 
               adtsheadersize = 
                  mAacUtils.SetADTSHeader(adtsheader, RDKAACUtils::MAX_BUFFER_SIZE, pPayload->length);
               if ( adtsheadersize )
#endif
               {
                  uint32_t adtsFormatStreamLen = adtsheadersize + pPayload->length;
                  uint8_t * adtsFormatStream= (uint8_t *) malloc(adtsFormatStreamLen);
#if defined ADD_ADTS_HEADER 
                  memcpy(adtsFormatStream,adtsheader,adtsheadersize);
#endif
                  memcpy(adtsFormatStream+adtsheadersize,pPayload->data,pPayload->length);
                  PushParsedAACBuffer(adtsFormatStream,adtsFormatStreamLen);
                  free(adtsFormatStream);
               }
            }	
            pPayload->Release();
            result =true ;
         }
         break;
      default:
         {
            LogI(">>[WARN] %s : Unknown Audio frame type Dropped!!\n", __FUNCTION__); 
            pPayload->Release();
            result =true ;
         }
         break;
   }
   return result;
}

bool RDKGstAVDecoder::SPSPPSSink(uint8_t* data, int32_t len, bool hwOK, bool firstChunk, bool lastChunk)
{
   PushParsedH264Buffer(data,len);
   return true;
}

bool RDKGstAVDecoder::H264SampleSink(TimeStamp time, uint8_t* data, int32_t len)
{
   PushParsedH264Buffer(data,len);
   return true;
}

void RDKGstAVDecoder::PushParsedH264Buffer(uint8_t * data, int32_t len)
{
   EsBuffer * newBuffer = NULL;
#if defined ENABLE_VIDEO_DECODE
   if(len)
   {
      newBuffer = (EsBuffer *) malloc(sizeof(EsBuffer));
      newBuffer->pts=mCurrentTimeStampVideo;
      newBuffer->size =len;
      newBuffer->type=kPTVideo;
      newBuffer->data = (char *) malloc(len);
      memcpy(newBuffer->data ,data,len);
      //writefile("videoOut.h264x",data,len);
      PushtoDecoder(newBuffer);
   }
#endif
}
void RDKGstAVDecoder::PushParsedAACBuffer(uint8_t * data, int32_t len)
{
   EsBuffer * newBuffer = NULL;
#if defined ENABLE_AUDIO_DECODE
   if(len)
   {
      newBuffer = (EsBuffer *) malloc(sizeof(EsBuffer));
      newBuffer->pts=mCurrentTimeStampAudio;//Correct this stamping
      newBuffer->size =len;
      newBuffer->type=kPTAudio;
      newBuffer->data = (char *) malloc(len);
      memcpy(newBuffer->data ,data,len);
      //writefile("audioOut.aacx",data,len);
      PushtoDecoder(newBuffer);
   }
#endif
}

typedef enum {
   GST_PLAY_FLAG_VIDEO                = 0x1,
   GST_PLAY_FLAG_AUDIO                = 0x2,
   GST_PLAY_FLAG_NATIVE_VIDEO         = 0x20,
   GST_PLAY_FLAG_NATIVE_AUDIO         = 0x40,        
   GST_PLAY_FLAG_BUFFER_AFTER_DEMUX   = 0x100
} GstPlayFlags;


void RDKGstAVDecoder::InitializeGstPipeline(void)
{
   LogI("%s:%d START GST INIT \n",__FUNCTION__,__LINE__);
   gint flags;
   mDecoderState = kStateStopped;
   gst_init (NULL, NULL);

   mPlaybin = gst_element_factory_make ("playbin", NULL);
   g_assert (mPlaybin);

   /* Set flags to show Audio and Video but ignore Subtitles */
#if defined BCM_SOC
   g_object_get (mPlaybin, "flags", &flags, NULL);
   flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO | GST_PLAY_FLAG_NATIVE_AUDIO;
   g_object_set (mPlaybin, "flags", flags, NULL);
#endif

   mBus = gst_pipeline_get_bus (GST_PIPELINE (mPlaybin));
   mBusWatch= gst_bus_create_watch(mBus);
   g_source_set_callback(mBusWatch, (GSourceFunc)GstBusCallback, this, NULL);
   g_source_attach(mBusWatch, mMainLoopThreadContext);

   g_object_set (mPlaybin, "uri", "dynappsrc://", NULL);

   g_signal_connect (mPlaybin, "deep-notify::source",
         (GCallback) PlaybinFoundSource, this);

   g_signal_connect( GST_BIN(mPlaybin), "element-added", G_CALLBACK(PipelineElementAdded), this );
   LogI(" RDKGSTAV GSTINIT DONE %s::%d\n",__FUNCTION__,__LINE__);

   return ;
}

GstBuffer * RDKGstAVDecoder::CreateGstBuffer( RDKGstAVDecoder::EsBuffer * esBuffer)
{
   GstBuffer *buffer = NULL;
   GstElement * appsrc = NULL;
   if(esBuffer)
   {
      if(esBuffer->type == kPTVideo)
      {
         appsrc = mVideoAppSrc;
      }
      else if(esBuffer->type == kPTAudio)
      {
         appsrc = mAudioAppSrc;
      }
      else
      {
         LogI(" RDKGSTAV Cannot Create Buffer ,Unknown PayLoad Type %s::%d type =%d\n",__FUNCTION__,__LINE__,esBuffer->type);
         return buffer;
      }

      buffer = gst_buffer_new_allocate (NULL, esBuffer->size, NULL);
      if (buffer != NULL) 
      {
         GstMapInfo gstMapInfo;
         if (gst_buffer_map (buffer, &gstMapInfo, GST_MAP_WRITE)) 
         {
            memcpy (gstMapInfo.data, esBuffer->data, esBuffer->size);
            gst_buffer_unmap (buffer, &gstMapInfo);
#if defined ENABLE_PTS_STAMPING 
            GST_BUFFER_PTS (buffer) = esBuffer->pts;
            LogI(" RDKGSTAV STAMPING ORIG GSTBUFFER %s::%d ====>>>PTS=%lu [%lu ms]\n",__FUNCTION__,__LINE__,GST_BUFFER_PTS(buffer), GST_TIME_AS_MSECONDS(GST_BUFFER_PTS(buffer)));
#endif
         }
         else 
         {
            LogI( "Error in CreateGstBuffer. Failed to map GST buffer");
            gst_buffer_unref (buffer);
         }
      }
   }
   return buffer;
}

void RDKGstAVDecoder::SetAppsrcCaps(PayloadType type)
{
#if defined BCM_SOC
   GstCaps *caps = NULL;
   GstElement * appsrc = NULL;
   if(type == kPTVideo)
   {
      caps = gst_caps_new_simple("video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream", NULL);
      appsrc = mVideoAppSrc;
   }
   else
   {
      caps = gst_caps_new_simple("audio/x-aac",  NULL);
      appsrc = mAudioAppSrc;
   }
   g_object_set(appsrc, "caps", caps, NULL);
   gst_caps_unref(caps);
#endif
}

void RDKGstAVDecoder::PlaybinFoundSource(GObject * object, GObject * orig, GParamSpec * pspec, void * user)
{
   LogI(" RDKGSTAV GOT PLAYBIN SOURCE %s::%d\n",__FUNCTION__,__LINE__);
   RDKGstAVDecoder * avDecoder = (RDKGstAVDecoder *) user;
   GstElement *dynappsrc = NULL;
   GstElement *videoAppSrc= NULL;
   GstElement *audioAppSrc= NULL;
   g_object_get (orig, pspec->name, &dynappsrc, NULL);
   avDecoder->SetDynAppsrc(dynappsrc);

#if defined ENABLE_VIDEO_DECODE
   g_signal_emit_by_name (dynappsrc, "new-appsrc", "video", &videoAppSrc);
   avDecoder->SetAppsrc(videoAppSrc,kPTVideo);
   gst_app_src_set_stream_type((GstAppSrc *)(videoAppSrc), GST_APP_STREAM_TYPE_STREAM);
   g_object_set(G_OBJECT(videoAppSrc), "emit-signals", false, NULL);
   g_object_set(G_OBJECT(videoAppSrc), "max-bytes", gint64(VIDEO_APPSRC_FIFO_SIZE), NULL);
   g_object_set (G_OBJECT (videoAppSrc),"stream-type", GST_APP_STREAM_TYPE_STREAM ,"format", GST_FORMAT_TIME,"is-live", TRUE,NULL);
   //g_object_set(G_OBJECT(videoAppSrc), "block", TRUE, NULL);
   avDecoder->SetAppsrcCaps(kPTVideo);
#endif

#if defined ENABLE_AUDIO_DECODE
   g_signal_emit_by_name (dynappsrc, "new-appsrc", "audio", &audioAppSrc);
   gst_object_ref (audioAppSrc);
   avDecoder->SetAppsrc(audioAppSrc,kPTAudio);
   gst_app_src_set_stream_type((GstAppSrc *)(audioAppSrc), GST_APP_STREAM_TYPE_STREAM);
   g_object_set(G_OBJECT(audioAppSrc), "emit-signals", false, NULL);
   g_object_set(G_OBJECT(audioAppSrc), "max-bytes", gint64(AUDIO_APPSRC_FIFO_SIZE), NULL);
   g_object_set (G_OBJECT (audioAppSrc),"stream-type", GST_APP_STREAM_TYPE_STREAM,"format", GST_FORMAT_TIME,"is-live", TRUE,NULL);
   //g_object_set(G_OBJECT(audioAppSrc), "block", TRUE, NULL);
   avDecoder->SetAppsrcCaps(kPTAudio);
#endif
   LogI(" RDKGSTAV GOT PLAYBIN SOURCE cOMPLETE %s::%d\n",__FUNCTION__,__LINE__);
}

char *RDKGstAVDecoder::GetPipelineStateStr(GstState state)
{
   static char *state_str[] = {(char *)"GST_STATE_VOID_PENDING",
      (char *)"GST_STATE_NULL",
      (char *)"GST_STATE_READY",
      (char *)"GST_STATE_PAUSED",
      (char *)"GST_STATE_PLAYING"};
   return state_str[state];
}

void RDKGstAVDecoder::SendEOF(void)
{
   LogI( "SEND_EOS START");
   gst_object_unref (mAudioAppSrc);
   gst_object_unref (mVideoAppSrc);
   if (mStreamMetadata.audioCodec != kACNoAudio ) 
      mNotifier->NotifyEOF(kPTAudio);
   if (mStreamMetadata.videoCodec != kVCNoVideo ) 
      mNotifier->NotifyEOF(kPTVideo);
   LogI( "SEND_EOS COMPLETE");
}


gboolean RDKGstAVDecoder::ChangePipelineState(DecState state)
{
   gint64  endTime;
   gboolean ret = TRUE;
   gboolean wait = TRUE;
   GstState gstState; 
   DecState finalState = mDecoderState;
   GstStateChangeReturn sret;

   if(state != mDecoderState)
   {
      switch (state)
      {
         case kStatePlay:
            {
               gstState = GST_STATE_PLAYING;
               finalState = kStatePlaying;
            }
            break;
         case kStatePause:
            {
               gstState = GST_STATE_PAUSED;
               finalState = kStatePaused;
            }
            break;
         default:
            LogI("***********  RDKGSTAV UNKNOWN DECODER STATE TRANSTITION REQUESTED %s::%d*********%d****\n",__FUNCTION__,__LINE__,state);
            break;
      }


      //Should not Wait for Transition to Playing when pipeline has just been created or flushed for seek ,since buffer pushes are required
      //to transition the pipeline to PLAYING.

      wait = !(mDecoderState == kStatePreroll);
      g_mutex_lock(&mPipelineMutex);
      sret = gst_element_set_state (mPlaybin, gstState);
      if(sret != GST_STATE_CHANGE_FAILURE)
      { 
         LogI(" RDKGSTAV STATE CHANGE ret %d %s::%d reqState %d decState %d finalState %d wait %d\n",sret,__FUNCTION__,__LINE__,state,mDecoderState,finalState, wait);
         if(wait)
         {
            LogI(" RDKGSTAV STATE CHANGE WAITING %s::%d reqState %d decState %d\n",__FUNCTION__,__LINE__,state,mDecoderState);
            endTime = g_get_monotonic_time () + G_TIME_SPAN_SECOND * GST_STATE_CHANGE_WAIT_SECS ;
            if ( FALSE == g_cond_wait_until(&mStateCondition, &mPipelineMutex, endTime) )  
            {
               LogI(" RDKGSTAV STATE CHANGE TIMEDOUT %s::%d\n",__FUNCTION__,__LINE__);
               ret = FALSE;
            }
            if(mDecoderState != finalState)
            {
               LogI(" RDKGSTAV STATE CHANGE DOES NOT MATCH %s::%d reqState %d decState %d finalState %d\n",__FUNCTION__,__LINE__,state,mDecoderState,finalState);
               ret = FALSE;
            }
         }
      }
      else
      {
         LogI(" RDKGSTAV STATE CHANGE RETURNED FAILURE %s::%d\n",__FUNCTION__,__LINE__);
         ret = FALSE;
      }
      g_mutex_unlock(&mPipelineMutex);
   }
   else
   {
      LogI(" RDKGSTAV STATE CHANGE ALREADY COMPLETE %s::%d reqState %d decState %d\n",__FUNCTION__,__LINE__,state,mDecoderState);
   }
   return ret;
}

void RDKGstAVDecoder::NotifyPipelineStateChange(GstState state)
{
   if((state == GST_STATE_PAUSED) || (state == GST_STATE_PLAYING))
   {
      g_mutex_lock (&mPipelineMutex);
      if(state == GST_STATE_PLAYING)
      {
         mDecoderState = kStatePlaying;
      }
      else if(state == GST_STATE_PAUSED)
      {
         mDecoderState = kStatePaused;
      }
      LogI( "STATE TRANSITION COMPLETE %d",mDecoderState);
      g_cond_signal (&mStateCondition);
      g_mutex_unlock (&mPipelineMutex);
   }
}


#define GRAPH_DEBUG_LEVEL GstDebugGraphDetails(GST_DEBUG_GRAPH_SHOW_ALL)
gboolean RDKGstAVDecoder::GstBusCallback(GstBus *bus, GstMessage *msg, gpointer data)
{
   RDKGstAVDecoder * avDecoder= (RDKGstAVDecoder *) data;
   GstElement *  pipeline = avDecoder->GetPipeline();
   //LogI(" RDKGSTAV GST BUS CALL %s::%d element %s type %s\n",__FUNCTION__,__LINE__,GST_MESSAGE_SRC_NAME(msg),GST_MESSAGE_TYPE_NAME(msg));

   switch (GST_MESSAGE_TYPE (msg)) 
   {
      case GST_MESSAGE_EOS:
         {
            LogI("EOS from element %s: %s\n",
                  GST_OBJECT_NAME (msg->src) );
            gboolean eos = avDecoder->CheckEosReceived(); 
            if((GST_ELEMENT(msg->src) == pipeline) && eos)
            {
               LogI("NOTIFY EOS from element %s: %s\n",
                     GST_OBJECT_NAME (msg->src) );
               avDecoder->SendEOF();
            }
         }
         break;
      case GST_MESSAGE_ERROR:
         {
            GError *err = NULL;
            gchar *dbg_info = NULL;

            gst_message_parse_error (msg, &err, &dbg_info);
            LogI("ERROR from element %s: %s\n",
                  GST_OBJECT_NAME (msg->src), err->message);
            LogI("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
            g_error_free (err);
            g_free (dbg_info);
         }
         break;
      case GST_MESSAGE_WARNING:
         {
            GError *err = NULL;
            gchar *dbg_info = NULL;

            gst_message_parse_warning(msg, &err, &dbg_info);
            LogI("WARNING from element %s: %s\n",
                  GST_OBJECT_NAME (msg->src), err->message);
            LogI("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
            g_error_free (err);
            g_free (dbg_info);
         }
         break;
      case GST_MESSAGE_INFO:
         {
            GError *err = NULL;
            gchar *dbg_info = NULL;

            gst_message_parse_info(msg, &err, &dbg_info);
            LogI("INFO from element %s: %s\n",
                  GST_OBJECT_NAME (msg->src), err->message);
            LogI("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
            g_error_free (err);
            g_free (dbg_info);
         }
         break;
      case GST_MESSAGE_STATE_CHANGED:
         {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed( msg, &old_state, &new_state, &pending_state );
            LogI("RDKGstAVDecoder ELEMENT %s State change complete %d-->%d\n",GST_MESSAGE_SRC_NAME(msg), old_state,new_state);
            if( GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline) )
            {
               LogI("RDKGstAVDecoder Pipeline State change complete %d-->%d\n", old_state,new_state);
               {
                  LogI("RDKGstAVDecoder  Pipeline State PLAYING signalling %d-->%d\n", old_state,new_state);
                  LogI( "BUS CB SIGNAL pthread %d",pthread_self());
                  avDecoder->NotifyPipelineStateChange(new_state);
               }
            }
            if (msg->src == (GstObject *)pipeline)
            {
               if (0) 
               {
                  // create time stamped DOT files on state changes
                  time_t t = time(0);
                  const struct tm *now = localtime(&t);
                  char time[50];
                  char dotName[50];
                  strftime(time, sizeof(time), "%H-%M-%S", now);
                  snprintf (dotName, sizeof(dotName), "x%s-%s", avDecoder->GetPipelineStateStr(new_state), time);
                  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GRAPH_DEBUG_LEVEL, dotName);
               }
            }                        
         }
         break;
      default:
         break;
   }
   return TRUE;
}

void RDKGstAVDecoder::PipelineElementAdded(GstBin *bin, GstElement *element, gpointer  data)
{
   RDKGstAVDecoder * avDecoder= (RDKGstAVDecoder *) data;
   LogI("on_auto_element_added() new Element: %s !!\n", GST_ELEMENT_NAME(element));
   if ( GST_IS_BIN(element) )
   {
      LogI( "%s : Element is a BIN of name (%s) ! it can have sub-elements ...",__FUNCTION__, GST_ELEMENT_NAME(element));
      g_signal_connect( element, "element-added", G_CALLBACK(PipelineElementAdded), data);
   }
#if defined BCM_SOC 
   gchar *name = gst_element_get_name( element );
   if ( NULL != strstr((const char *)name, "brcmvideodecoder") )
   {
      avDecoder->SetDecoderElement(element, kPTVideo);
      g_object_set(G_OBJECT(element), "sync-off", true, NULL);
      LogI("RDKGSTAV SETTING VIDEO FREE RUNNING %s::%d \n",__FUNCTION__,__LINE__);
   }
   else if ( NULL != strstr((const char *)name, "brcmaudiodecoder") )
   {
      avDecoder->SetDecoderElement(element, kPTAudio);
      g_object_set(G_OBJECT(element), "sync-off", true, NULL);
      LogI("RDKGSTAV SETTING AUDIO FREE RUNNING  %s::%d \n",__FUNCTION__,__LINE__);
   }
#endif
}




void RDKGstAVDecoder::PushtoDecoder(EsBuffer * newEsBuffer)
{
   static int pushfailcount = 0;
   static int pushsucccount = 0;
   if(newEsBuffer)
   {
      GstBuffer *gstBuf = NULL;
      guint64 avail;
      gsize  bufSize;
      GstElement * appsrc = NULL;

      g_mutex_lock (&mPipelineMutex);
      if((mDecoderState == kStatePreroll) || (mDecoderState == kStatePlaying)) 
      {
         if(newEsBuffer->type == kPTVideo)
            appsrc = mVideoAppSrc;
         else if(newEsBuffer->type == kPTAudio)
            appsrc = mAudioAppSrc;

         gstBuf = CreateGstBuffer( newEsBuffer);

         if(gstBuf && appsrc)
         {
            LogI(" RDKGSTAV GOT GST BUFFER %s::%d TYPE %d ,BUF PTS %lu \n",__FUNCTION__,__LINE__,newEsBuffer->type,GST_BUFFER_PTS(gstBuf));
            GstFlowReturn flow;

            pushsucccount++;
            flow = gst_app_src_push_buffer (GST_APP_SRC (appsrc), gstBuf);
            LogI(" RDKGSTAV %s::%d PUSH COMPLETE %d \n",__FUNCTION__,__LINE__,pushsucccount);
            if (flow != GST_FLOW_OK)
            {
               pushfailcount++;
               LogI( "Error PIPLEINE PUSH FAILED !!! %d",pushfailcount );
            }
         }
      }
      else
      {
         pushfailcount++;
         LogI( " Error PIPLEINE STATE IS NOT PLAYING !!! %d",pushfailcount );
      }
      g_mutex_unlock (&mPipelineMutex);
      free(newEsBuffer->data);
      free(newEsBuffer);
   }
}

