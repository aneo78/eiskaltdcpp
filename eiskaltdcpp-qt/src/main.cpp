#include <stdlib.h>
#include <iostream>
#include <string>

using namespace std;

#include "dcpp/stdinc.h"
#include "dcpp/DCPlusPlus.h"

#include "dcpp/forward.h"
#include "dcpp/QueueManager.h"
#include "dcpp/HashManager.h"
#include "dcpp/Thread.h"

#include "MainWindow.h"
#include "WulforUtil.h"
#include "WulforSettings.h"
#ifdef USE_LIBUPNP
#include "UPnP.h"
#include "UPnPMapper.h"
#endif
#include "HubManager.h"
#include "Notification.h"
#include "SingleInstanceRunner.h"
#include "Version.h"
#include "EmoticonFactory.h"

#ifdef USE_ASPELL
#include "SpellCheck.h"
#endif

#ifdef USE_JS
#include "ScriptEngine.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QRegExp>

#ifdef DBUS_NOTIFY
#include <QtDBus>
#endif

void callBack(void* x, const std::string& a)
{
    std::cout << "Loading: " << a << std::endl;
}

void parseCmdLine(const QStringList &);

#ifndef WIN32
#include <unistd.h>
#include <signal.h>

void installHandlers();

#ifdef FORCE_XDG
#include <QTextStream>
void migrateConfig();
#endif

#endif

int main(int argc, char *argv[])
{
    EiskaltApp app(argc, argv);
    int ret = 0;

    parseCmdLine(qApp->arguments());

    SingleInstanceRunner runner;

    if (runner.isServerRunning(qApp->arguments()))
        return 0;

#ifndef WIN32
    installHandlers();
#endif

#ifdef FORCE_XDG
    migrateConfig();
#endif

    dcpp::startup(callBack, NULL);
    dcpp::TimerManager::getInstance()->start();

    HashManager::getInstance()->setPriority(Thread::IDLE);

    WulforSettings::newInstance();
    WulforSettings::getInstance()->load();
    WulforSettings::getInstance()->loadTranslation();
    WulforSettings::getInstance()->loadTheme();

    WulforUtil::newInstance();

    Text::hubDefaultCharset = WulforUtil::getInstance()->qtEnc2DcEnc(WSGET(WS_DEFAULT_LOCALE)).toStdString();

    if (WulforUtil::getInstance()->loadUserIcons())
        std::cout << "UserList icons has been loaded" << std::endl;

    if (WulforUtil::getInstance()->loadIcons())
        std::cout << "Application icons has been loaded" << std::endl;
#ifdef USE_LIBUPNP
    UPnP::newInstance();
    UPnP::getInstance()->start();
    UPnPMapper::newInstance();
#endif
    HubManager::newInstance();

    MainWindow::newInstance();
    MainWindow::getInstance()->setUnload(!WBGET(WB_TRAY_ENABLED));

    WulforSettings::getInstance()->loadTheme();

    if (WBGET(WB_APP_ENABLE_EMOTICON)){
        EmoticonFactory::newInstance();
        EmoticonFactory::getInstance()->load();
    }

#ifdef USE_ASPELL
    if (WBGET(WB_APP_ENABLE_ASPELL))
        SpellCheck::newInstance();
#endif

    Notification::newInstance();
    Notification::getInstance()->enableTray(WBGET(WB_TRAY_ENABLED));

    if (!WBGET(WB_MAINWINDOW_HIDE) || !WBGET(WB_TRAY_ENABLED))
        MainWindow::getInstance()->show();

    MainWindow::getInstance()->autoconnect();
    MainWindow::getInstance()->parseCmdLine();

#ifdef USE_JS
    ScriptEngine::newInstance();
#endif

    ret = app.exec();

    std::cout << "Shutting down libdcpp..." << std::endl;

    WulforSettings::getInstance()->save();

    EmoticonFactory::deleteInstance();

#ifdef USE_ASPELL
    if (SpellCheck::getInstance())
        SpellCheck::deleteInstance();
#endif
#ifdef USE_LIBUPNP
    UPnPMapper::deleteInstance();
    UPnP::getInstance()->stop();
    UPnP::deleteInstance();
#endif
    Notification::deleteInstance();

#ifdef USE_JS
    ScriptEngine::deleteInstance();
#endif

    MainWindow::deleteInstance();

    HubManager::deleteInstance();

    WulforUtil::deleteInstance();
    WulforSettings::deleteInstance();

    dcpp::shutdown();

    std::cout << "Quit..." << std::endl;

    runner.servStop();

    return ret;
}

void parseCmdLine(const QStringList &args){
    foreach (QString arg, args){
        if (arg == "-h" || arg == "--help"){
            About().printHelp();

            exit(0);
        }
        else if (arg == "-v" || arg == "--version"){
            About().printVersion();

            exit(0);
        }
    }
}

#ifndef WIN32
void installHandlers(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1){
        std::cout << "Cannot handle SIGPIPE" << std::endl;
    }

    std::cout << "Signal handlers installed." << std::endl;
}

#endif

#ifdef FORCE_XDG

void copy(const QDir &from, const QDir &to){
    if (!from.exists() || to.exists())
        return;

    QString to_path = to.absolutePath();
    QString from_path = from.absolutePath();

    if (!to_path.endsWith(QDir::separator()))
        to_path += QDir::separator();

    if (!from_path.endsWith(QDir::separator()))
        from_path += QDir::separator();

    foreach (QString s, from.entryList(QDir::Dirs)){
        QDir new_dir(to_path+s);

        if (new_dir.exists())
            continue;
        else{
            if (!new_dir.mkpath(new_dir.absolutePath()))
                continue;

            copy(QDir(from_path+s), new_dir);
        }
    }

    foreach (QString f, from.entryList(QDir::Files)){
        QFile orig(from_path+f);

        if (!orig.copy(to_path+f))
            continue;
    }
}

void migrateConfig(){
    const char* home_ = getenv("HOME");
    string home = home_ ? Text::toUtf8(home_) : "/tmp/";
    string old_config = home + "/.eiskaltdc++/";

    const char *xdg_config_home_ = getenv("XDG_CONFIG_HOME");
    string xdg_config_home = xdg_config_home_? Text::toUtf8(xdg_config_home_) : (home+"/.config");
    string new_config = xdg_config_home + "/eiskaltdc++/";

    if (!QDir().exists(old_config.c_str()) || QDir().exists(new_config.c_str())){
        if (!QDir().exists(new_config.c_str())){
            old_config = _DATADIR + string("/config/");

            if (!QDir().exists(old_config.c_str()))
                return;
        }
        else
            return;
    }

    try{
        printf("Migrating to XDG paths...\n");

        copy(QDir(old_config.c_str()), QDir(new_config.c_str()));

        QFile orig(new_config.c_str()+QString("DCPlusPlus.xml"));
        QFile new_file(new_config.c_str()+QString("DCPlusPlus.xml.new"));

        if (!(orig.open(QIODevice::ReadOnly | QIODevice::Text) && new_file.open(QIODevice::WriteOnly | QIODevice::Text))){
            orig.close();
            new_file.close();

            printf("Migration failed.\n");

            return;
        }

        QTextStream rstream(&orig);
        QTextStream wstream(&new_file);

        QRegExp replace_str("/(\\S+)/\\.eiskaltdc\\+\\+/");
        QString line = "";

        while (!rstream.atEnd()){
            line = rstream.readLine();

            line.replace(replace_str, QString(new_config.c_str()));

            wstream << line << "\n";
        }

        wstream.flush();

        orig.close();
        new_file.close();

        orig.remove();
        new_file.rename(orig.fileName());

        printf("Ok. Migrated.\n");
    }
    catch(const std::exception&){
        printf("Migration failed.\n");
    }
}
#endif
