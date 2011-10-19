#include <QString>
#include <QDir>
#include <QList>
#include <QPair>
#include <QCoreApplication>

#include "ADBootstrap.h"

/******************************************************************************/

static const QByteArray s_bootstrapDirData = "bootstrap";
static const QByteArray s_bootstrapDir( s_bootstrapDirData );

static const QList<QPair<QString,QString> > s_bootBins = QList<QPair<QString,QString> >()
    << QPair<QString,QString>(":ADAPI.dll", "ADAPI.dll")

#ifndef _WIN_ // Unix
    << QPair<QString,QString>(":ADAPIServer.exe", "ADAPIServer.exe")
    << QPair<QString,QString>(":ADAPIServer.exe.so", "ADAPIServer.exe.so")
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
    for ( QList<QPair<QString,QString> >::ConstIterator it = s_bootBins.begin();
          it != s_bootBins.end();
          ++it ) {
        const QPair<QString, QString>& bin = *it;
        QFile in( bin.first );
        res = in.open( QFile::ReadOnly );
        if ( ! res ) {
            qWarning("Can't read bootstap bin '%s'",
                     qPrintable(bin.first));
            return res;
        }

        QFile out( bootstrapDir.absolutePath() + "/" + bin.second );
        res = out.open( QFile::WriteOnly );
        if ( ! res ) {
            qWarning("Can't write bootstap bin '%s'",
                     qPrintable(bin.second));
            return res;
        }

        QByteArray ba = in.readAll();
        qint64 size = out.write( ba );
        if ( ba.size() != size ) {
            qWarning("Write to bootstrap bin '%s' failed",
                     qPrintable(bin.second));
            return false;
        }

        // Set executable permissions
        res = out.setPermissions( QFile::ReadOwner |
                                  QFile::WriteOwner |
                                  QFile::ExeOwner );
        if ( ! res ) {
            qWarning("Setting executable permission for bootstrap bin '%s' failed",
                     qPrintable(bin.second));
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
