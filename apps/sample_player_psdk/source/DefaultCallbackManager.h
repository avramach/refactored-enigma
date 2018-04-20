#include <glib.h>

#include "psdk/PSDK.h"
#include "psdkutils/PSDKError.h"
#include "psdkutils/PSDKTypes.h"

extern void LogI(const char *format, ... );
using namespace psdk;

class DefaultCallbackManager : public psdk::PSDKEventManager
{
   public:
      virtual void eventPosted() 
      {
         //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
         g_idle_add(ThreadCustomMessageHandler, this);
         //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
      }
   private:

      static gboolean ThreadCustomMessageHandler(gpointer user_data)
      {
         DefaultCallbackManager* mgr =  (DefaultCallbackManager*)user_data;
         if (mgr)
         {  
            //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
            mgr->firePostedEvents();             
            //LogI(" RDKAV %s:%d\n",__FUNCTION__,__LINE__);
         }

         return FALSE; // don't call again
      }


   public:
      virtual psdkutils::PSDKErrorCode getInterface(psdkutils::InterfaceId id, void **retValue)
      {
         return psdkutils::kECInterfaceNotFound;
      }			

      DefaultCallbackManager()
         : _refCount(0)
      {
      }

      PSDKSHAREDPOINTER_METHODS
         PSDKUSERDATA_METHODS
};

