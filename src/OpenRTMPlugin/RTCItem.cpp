/**
   @author Shizuko Hattori
   @author Shin'ichiro Nakaoka
*/

#include "RTCItem.h"
#include "OpenRTMUtil.h"
#include <cnoid/BodyItem>
#include <cnoid/Link>
#include <cnoid/BasicSensors>
#include <cnoid/ItemManager>
#include <cnoid/Archive>
#include <cnoid/FileUtil>
#include <cnoid/ExecutablePath>
#include <cnoid/MessageView>
#include <cnoid/Sleep>
#include <cnoid/ProjectManager>
#include <rtm/RTObject.h>
#include <rtm/CorbaNaming.h>
#include <boost/regex.hpp>
#include "gettext.h"

using namespace std;
using namespace std::placeholders;
using namespace cnoid;
namespace filesystem = boost::filesystem;

#if ( defined ( WIN32 ) || defined ( _WIN32 ) || defined(__WIN32__) || defined(__NT__) )
#define SUFFIX_SHARED_EXT   ".dll"
#define SUFFIX_EXE_EXT      ".exe"
#elif defined(__APPLE__)
#define SUFFIX_SHARED_EXT   ".dylib"
#define SUFFIX_EXE_EXT      ".app"
#else
#define SUFFIX_SHARED_EXT   ".so"
#define SUFFIX_EXE_EXT      ""
#endif

namespace {
const bool TRACE_FUNCTIONS = false;
}


void RTCItem::initialize(ExtensionManager* ext)
{
    static bool initialized = false;
    if(!initialized){
        ext->itemManager().registerClass<RTCItem>(N_("RTCItem"));
        ext->itemManager().addCreationPanel<RTCItem>();
        initialized = true;
    }
}


RTCItem::RTCItem()
    : os(MessageView::instance()->cout()),
      periodicType(N_PERIODIC_TYPE, CNOID_GETTEXT_DOMAIN_NAME),
      pathBase(N_PATH_BASE, CNOID_GETTEXT_DOMAIN_NAME)
{
    rtcomp = 0;
    moduleName.clear();
    mv = MessageView::instance();

    periodicRate = 1000;
    periodicType.setSymbol(PERIODIC_EXECUTION_CONTEXT,  N_("PeriodicExecutionContext"));
    periodicType.setSymbol(SYNCH_EXT_TRIGGER,  N_("SynchExtTriggerEC"));
    periodicType.setSymbol(EXT_TRIG_EXECUTION_CONTEXT,  N_("ExtTrigExecutionContext"));
    periodicType.setSymbol(CHOREONOID_EXECUTION_CONTEXT,  N_("ChoreonoidExecutionContext"));
    periodicType.select(PERIODIC_EXECUTION_CONTEXT);
    oldType = PERIODIC_EXECUTION_CONTEXT;
    properties.clear();
    properties.insert(make_pair(string("exec_cxt.periodic.type"), periodicType.selectedSymbol()));
    stringstream ss;
    ss << periodicRate;
    properties.insert(make_pair(string("exec_cxt.periodic.rate"), ss.str()));
    pathBase.setSymbol(RTC_DIRECTORY, N_("RTC directory"));
    pathBase.setSymbol(PROJECT_DIRECTORY, N_("Project directory"));
    pathBase.select(RTC_DIRECTORY);
    oldPathBase = RTC_DIRECTORY;
}


RTCItem::RTCItem(const RTCItem& org)
    : Item(org),
      os(MessageView::instance()->cout()),
      periodicType(org.periodicType),
      pathBase(org.pathBase)
{
    rtcomp = org.rtcomp;
    moduleName = org.moduleName;
    mv = MessageView::instance();
    periodicRate = org.periodicRate;
    oldType = org.oldType;
    properties = org.properties;
    oldPathBase = org.oldPathBase;
}

RTCItem::~RTCItem()
{
    
}


void RTCItem::onPositionChanged()
{
    if(!rtcomp){
        if(convertAbsolutePath())
            rtcomp = new RTComponent(modulePath, properties);
    }
}


void RTCItem::onDisconnectedFromRoot()
{
    if(rtcomp){
        rtcomp->deleteRTC();
        delete rtcomp;
        rtcomp = 0;
    }
}


Item* RTCItem::doDuplicate() const
{
    return new RTCItem(*this);
}


void RTCItem::setModuleName(const std::string& name)
{
    if(moduleName!=name){
        moduleName = name;
        if(rtcomp){
            delete rtcomp;
        }
        if (convertAbsolutePath())
            rtcomp = new RTComponent(modulePath, properties);
    }
}


void RTCItem::setPeriodicType(int type)
{
    if(oldType != type){
        oldType = type;
        properties["exec_cxt.periodic.type"] = periodicType.symbol(type);
        if(rtcomp){
            delete rtcomp;
        }
        if (convertAbsolutePath())
            rtcomp = new RTComponent(modulePath, properties);
    }
}


void RTCItem::setPeriodicRate(int rate)
{
    if(periodicRate!=rate){
        periodicRate = rate;
        stringstream ss;
        ss << periodicRate;
        properties["exec_cxt.periodic.rate"] = ss.str();
        if(rtcomp){
            delete rtcomp;
        }
        if (convertAbsolutePath())
            rtcomp = new RTComponent(modulePath, properties);
    }
}


void RTCItem::setPathBase(int base)
{
    pathBase.select(base);
    if (oldPathBase != base){
        oldPathBase = base;
        if (rtcomp){
            delete rtcomp;
        }
        if (convertAbsolutePath())
            rtcomp = new RTComponent(modulePath, properties);
    }
}


void RTCItem::doPutProperties(PutPropertyFunction& putProperty)
{
    Item::doPutProperties(putProperty);
    putProperty(_("Relative path base"), pathBase,
        std::bind(&RTCItem::setPathBase, this, _1), true);

    FileDialogFilter filter;
    filter.push_back( string(_(" Dynamic Link Library ")) + DLLSFX );
    string dir;
    if(!moduleName.empty() && checkAbsolute(filesystem::path(moduleName)))
        dir = filesystem::path(moduleName).parent_path().string();
    else{
        if(pathBase.is(RTC_DIRECTORY))
            dir = (filesystem::path(executableTopDirectory()) / CNOID_PLUGIN_SUBDIR / "rtc").string();
    }
    putProperty(_("RTC module name"), FilePath(moduleName, filter, dir),
                std::bind(&RTCItem::setModuleName, this, _1), true);

    putProperty(_("Execution context"), periodicType,
                std::bind((bool(Selection::*)(int))&Selection::select, &periodicType, _1));
    setPeriodicType(periodicType.selectedIndex());
    putProperty(_("Periodic rate"), periodicRate,
                std::bind(&RTCItem::setPeriodicRate, this, _1), true);
}


bool RTCItem::store(Archive& archive)
{
    if(!Item::store(archive)){
        return false;
    }
    archive.writeRelocatablePath("moduleName", moduleName);
    archive.write("periodicType", periodicType.selectedSymbol());
    archive.write("periodicRate", periodicRate);
    archive.write("relativePathBase", pathBase.selectedSymbol(), DOUBLE_QUOTED);
    return true;
}


bool RTCItem::restore(const Archive& archive)
{
    if(!Item::restore(archive)){
        return false;
    }
    string value;
    if(archive.read("moduleName", value)){
        filesystem::path path(archive.expandPathVariables(value));
        moduleName = getNativePathString(path);
    }
    string symbol;
    if(archive.read("periodicType", symbol)){
        periodicType.select(symbol);
        oldType = periodicType.selectedIndex();
        properties["exec_cxt.periodic.type"] = symbol;
    }
    if(archive.read("periodicRate", periodicRate)){
        stringstream ss;
        ss << periodicRate;
        properties["exec_cxt.periodic.rate"] = ss.str();
    }
    if(archive.read("relativePathBase", symbol) || archive.read("RelativePathBase", symbol)){
        pathBase.select(symbol);
        oldPathBase = pathBase.selectedIndex();
    }
    return true;
}


bool RTCItem::convertAbsolutePath()
{
    modulePath = moduleName;
    if (!checkAbsolute(modulePath)){
        if (pathBase.is(RTC_DIRECTORY)){
            modulePath = filesystem::path(executableTopDirectory()) / CNOID_PLUGIN_SUBDIR / "rtc" / modulePath;
        } else {
            string projectFile = ProjectManager::instance()->currentProjectFile();
            if (projectFile.empty()){
                mv->putln(_("Please save the project."));
                return false;
            } else {
                modulePath = boost::filesystem::path(projectFile).parent_path() / modulePath;
            }
        }
    }
    return true;
}


RTComponent::RTComponent(const filesystem::path& modulePath, PropertyMap& prop)
{
    rtc_ = 0;
    rtcRef = 0;
    if (modulePath.empty()){
        return;
    }
    init(modulePath, prop);
}


RTComponent::~RTComponent()
{
    deleteRTC();
}


void RTComponent::init(const filesystem::path& modulePath_, PropertyMap& prop)
{
    mv = MessageView::instance();
    modulePath = modulePath_;
    createRTC(prop);
}


bool  RTComponent::createRTC(PropertyMap& prop)
{   
    string moduleNameLeaf = modulePath.leaf().string();
    size_t i = moduleNameLeaf.rfind('.');
    if(i != string::npos){
        componentName = moduleNameLeaf.substr(0, i);
    } else {
        componentName = moduleNameLeaf;
    }

    string actualFilename;

    if(filesystem::exists(modulePath)){
        actualFilename = getNativePathString(modulePath);
        if(modulePath.extension() == SUFFIX_SHARED_EXT){
            string initFunc(componentName + "Init");
            setupModules(actualFilename, initFunc, componentName, prop);          
        } else {
            createProcess(actualFilename, prop);
        }   
    } else {
        filesystem::path exePath(modulePath.string() + SUFFIX_EXE_EXT);
        if(filesystem::exists(exePath)){
            actualFilename = getNativePathString(exePath);
            createProcess(actualFilename, prop);
        } else {
            filesystem::path dllPath(modulePath.string() + SUFFIX_SHARED_EXT);
            if(filesystem::exists(dllPath)){
                actualFilename = getNativePathString(dllPath);
                string initFunc(componentName + "Init");
                setupModules(actualFilename, initFunc, componentName, prop);
            } else {
                mv->putln(fmt(_("A file of RTC \"%1%\" does not exist.")) % componentName);
            }
        }
    }

    bool created = isValid();

    if(created){
        mv->putln(fmt(_("RTC \"%1%\" has been created from \"%2%\".")) % componentName % actualFilename);
    } else {
        mv->putln(fmt(_("RTC \"%1%\" cannot be created.")) % componentName);
    }

    return created;
}


void RTComponent::setupModules(string& fileName, string& initFuncName, string& componentName, PropertyMap& prop)
{
    RTC::Manager& rtcManager = RTC::Manager::instance();

    rtcManager.load(fileName.c_str(), initFuncName.c_str());

    string option("?");
    for(PropertyMap::iterator it = prop.begin(); it != prop.end(); ){
        option += it->first + "=" + it->second;
        if(++it != prop.end()){
            option += "&";
        }
    }
    rtc_ = createManagedRTC((componentName + option).c_str());

    if(!rtc_){
        mv->putln(fmt(_("RTC \"%1%\" cannot be created by the RTC manager.\n"
                        " RTC module file: \"%2%\"\n"
                        " Init function: %3%\n"
                        " option: %4%"))
                  % componentName % fileName % initFuncName % option);
    }
}


void RTComponent::onReadyReadServerProcessOutput()
{
    mv->put(QString(rtcProcess.readAll()));
}


bool RTComponent::isValid() const
{
    return (rtc_ || rtcProcess.state() != QProcess::NotRunning);
}


void RTComponent::createProcess(string& command, PropertyMap& prop)
{
    QStringList argv;
    argv.push_back(QString("-o"));
    argv.push_back(QString("naming.formats: %n.rtc"));
    argv.push_back(QString("-o"));
    argv.push_back(QString("logger.enable: NO"));
    for(PropertyMap::iterator it = prop.begin(); it != prop.end(); it++){
        argv.push_back(QString("-o"));
        argv.push_back(QString(string(it->first+":"+it->second).c_str()));
    }
    if(rtcProcess.state() != QProcess::NotRunning){
        rtcProcess.kill();
        rtcProcess.waitForFinished(100);
    }
#ifdef _WIN32
    rtcProcess.start(QString("\"") + command.c_str() + "\"", argv );
#else
    rtcProcess.start(command.c_str(), argv);
#endif
    if(!rtcProcess.waitForStarted()){
        mv->putln(fmt(_("RT Component process \"%1%\" cannot be executed.")) % command);
    } else {
        mv->putln(fmt(_("RT Component process \"%1%\" has been executed.")) % command );
        rtcProcess.sigReadyReadStandardOutput().connect(
            std::bind(&RTComponent::onReadyReadServerProcessOutput, this));
    }
}


void RTComponent::deleteRTC()
{
    if(TRACE_FUNCTIONS){
        cout << "BodyRTComponent::deleteRTC()" << endl;
    }
    
    if(rtc_){
        string rtcName(rtc_->getInstanceName());
        mv->putln(fmt(_("delete %1%")) % rtcName);
        if(!cnoid::deleteRTC(rtc_)){
            mv->putln(fmt(_("%1% cannot be deleted.")) % rtcName);
        }
        rtc_ = 0;

    } else if(rtcProcess.state() != QProcess::NotRunning){
        mv->putln(fmt(_("delete %1%")) % componentName);
        rtcProcess.kill();
        rtcProcess.waitForFinished(100);
    }
   
    if(TRACE_FUNCTIONS){
        cout << "End of BodyRTCItem::deleteModule()" << endl;
    }
}
