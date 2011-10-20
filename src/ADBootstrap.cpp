#include <QString>
#include <QDir>
#include <QList>
#include <QPair>
#include <QFile>
#include <QCoreApplication>

#include "ADBootstrap.h"

/******************************************************************************/

static const QByteArray s_bootstrapDirData = "bootstrap";
static const QByteArray s_bootstrapDir( s_bootstrapDirData );

#define RWX (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)
#define RW  (QFile::ReadOwner | QFile::WriteOwner)

typedef QPair<QString,QFile::Permissions> BootPair;

static const QList<BootPair> s_bootBins = QList<BootPair>()
    << BootPair("ADAPI.dll", RWX)

#ifndef _WIN_ // Unix
    << BootPair("ADAPIServer.exe", RWX)
    << BootPair("ADAPIServer.exe.so", RWX)
#endif
       ;

/******************************************************************************/

bool ADBootstrap::bootstrap ()
{
    bool res = false;

    // Create bootstrap directory
    QDir bootstrapDir( QCoreApplication::applicationDirPath() + "/" + s_bootstrapDir );
    if ( ! bootstrapDir.exists() ) {
        res = bootstrapDir.mkpath(bootstrapDir.absolutePath());
        if ( ! res ) {
           qWarning("Can't create bootstrap dir '%s'",
                    qPrintable(bootstrapDir.absolutePath()));
            return res;
        }
    }

    // Copy needed binaries
    for ( QList<BootPair>::ConstIterator it = s_bootBins.begin();
          it != s_bootBins.end();
          ++it ) {
        const BootPair& bin = *it;
        QFile in( ":" + bin.first );
        res = in.open( QFile::ReadOnly );
        if ( ! res ) {
            qWarning("Can't read bootstap bin '%s'",
                     qPrintable(bin.first));
            return res;
        }

        QFile out( bootstrapDir.absolutePath() + "/" + bin.first );
        res = out.open( QFile::WriteOnly );
        if ( ! res ) {
            qWarning("Can't write bootstap bin '%s'",
                     qPrintable(bin.first));
            return res;
        }

        QByteArray ba = in.readAll();
        qint64 size = out.write( ba );
        if ( ba.size() != size ) {
            qWarning("Write to bootstrap bin '%s' failed",
                     qPrintable(bin.first));
            return false;
        }

        // Set executable permissions
        res = out.setPermissions( bin.second );
        if ( ! res ) {
            qWarning("Setting executable permission for bootstrap bin '%s' failed",
                     qPrintable(bin.first));
            return res;
        }
    }

    return true;
}

const char* ADBootstrap::bootstrapDir ()
{
    return s_bootstrapDir.constData();
}

/******************************************************************************/
