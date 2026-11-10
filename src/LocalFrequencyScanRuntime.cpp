#ifdef ESPSOMFY_LOCAL_UI
#include "LocalFrequencyScan.h"
#include "FrequencyScanService.h"
#include "LocalOperationAdmission.h"
namespace LocalUi {namespace RadioScan {namespace {
class RuntimeBackend final:public Backend {
public:
 SecuritySnapshot authorization()override{auto auth=pollAuthorization();auth.allowed=auth.allowed&&!localControlsFrozen();return auth;}
 void activity()override{authorizationActivity();}
 ::FrequencyScan::Source capture()override{const auto auth=authorization();return ::FrequencyScan::localSource(auth.allowed?auth.generation:0,auth.allowed?auth.configurationGeneration:0);}
 bool begin(const ::FrequencyScan::Source &source,FrequencyScanToken &token)override{return ::FrequencyScan::begin(source,token);}
 bool stop(const ::FrequencyScan::Source &source,const FrequencyScanToken &token)override{return ::FrequencyScan::stop(source,token);}
 ::FrequencyScan::Snapshot status()override{return ::FrequencyScan::status();}
 bool peak(const ::FrequencyScan::Source &source,const FrequencyScanToken &token,float &frequency)override{return ::FrequencyScan::peak(source,token,frequency);}
};
}
Backend &runtimeBackend(){static RuntimeBackend value;return value;}
}}
#endif
