#include <QUrl>
#include <QMetaType>
#include <QReadLocker>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <QSqlRecord>
#include <QVariant>
#include <QTextCodec>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QAuthenticator>

#include "ADConnection.h"
#include "ADSubscription.h"
#include "ADOrder.h"
#include "ADBootstrap.h"
#include "ADTemplateParser.h"

#ifdef _WIN_
 #include "ADLocalLibrary.h"
#else
 #include "ADRemoteLibrary.h"
#endif

// Log everything to file!
#define DO_ALL_LOGGING 1

namespace
{
    static const QString AD_DB_PREFIX("AD_");

    static QHash<QString, QString> s_simpleFilters;
    static QSet<QString> s_complexFilters = QSet<QString>() <<
              "filter_A" << //asset trades stream
              "filter_B" << //asset updates
              "filter_C" << //chat
              "filter_f" << //quotes
              "filter_q" << //orders queue ("stakan")
              "filter_T";   //???

    static QHash<QString, QSet<QString> > s_tablesTimestamps;

    class InitHelper
    {
    public:
        InitHelper ()
        {
            /*
              s_simpleFilters.insert("???a",    "acc_bal");
              s_simpleFilters.insert("???b",    "acc_section_bal");
              s_simpleFilters.insert("???c",    "order_types");
              s_simpleFilters.insert("???d",    "portf_bal");
              s_simpleFilters.insert("???e",    "portf_section_bal");
              s_simpleFilters.insert("???f",    "rapid_limits");
              s_simpleFilters.insert("???g",    "rapid_orders");
            */
            s_simpleFilters.insert("*acc*",     "sub_accounts");
            s_simpleFilters.insert("*acs*",     "accounts");
            s_simpleFilters.insert("*act*",     "actives");
            s_simpleFilters.insert("*bl*",      "balance");
            s_simpleFilters.insert("*brd*",     "boards");
            s_simpleFilters.insert("*ctm*",     "chat_themes");
            s_simpleFilters.insert("*cur*",     "curr");
            s_simpleFilters.insert("*ect*",     "exchange_contragents");
            s_simpleFilters.insert("*emt*",     "emitents");
            s_simpleFilters.insert("*exc*",     "exchanges");
            s_simpleFilters.insert("*fst*",     "paper_statuses");
            s_simpleFilters.insert("*idt*",     "abnormal_dates");
            //SPAM s_simpleFilters.insert("*ns*",       "news");
            s_simpleFilters.insert("*ost*",     "order_statuses");
            s_simpleFilters.insert("*pap*",     "papers");
            s_simpleFilters.insert("*pos*",     "positions");
            s_simpleFilters.insert("*rq*",      "orders");
            s_simpleFilters.insert("*templ*",   "doc_templates");
            s_simpleFilters.insert("*tpl*",     "trade_places");
            s_simpleFilters.insert("*tr*",      "trades"); //only my trades! not for whole market!
            s_simpleFilters.insert("*trt*",     "trade_types");

            s_tablesTimestamps.insert("trades", QSet<QString>() << "ts_time" << "settlement_date");
            s_tablesTimestamps.insert("chat", QSet<QString>() << "end_date");
            s_tablesTimestamps.insert("orders", QSet<QString>() << "ts_time" << "last_date" << "actv_time" << "drop_time" << "updt_time");
            s_tablesTimestamps.insert("papers", QSet<QString>() << "cup_date" << "mat_date");
            s_tablesTimestamps.insert("actives", QSet<QString>() << "buyback_date");
            s_tablesTimestamps.insert("abnormal_dates", QSet<QString>() << "ab_date");
            s_tablesTimestamps.insert("news", QSet<QString>() << "db_data");
            s_tablesTimestamps.insert("trade_places", QSet<QString>() << "last_session_date" << "prev_session_date");
            s_tablesTimestamps.insert("historical_quotes", QSet<QString>() << "quote_timestamp");
        }
    };
    InitHelper initHelper__;
};

/****************************************************************************/

namespace ADBlockName
{
    // Server time
    const char* SRV_TIME = "*st*";

    // Broker response
    const char* MSG_ID = "*opmsgid*";

    // Orders queue for refresh rate: 0 (max), 1 (low)
    const char* ORDERS_QU_INIT = "*qinit*";
    const char* ORDERS_QU_0 = "*qu*";
    const char* ORDERS_QU_1 = "*q1*";

    // Quote for refresh rate: 0 (max), 1, 2, 3 (low)
    const char* QUOTE_INIT = "*finit*";
    const char* QUOTE_0 = "*f0*";
    const char* QUOTE_1 = "*f1*";
    const char* QUOTE_2 = "*f2*";
    const char* QUOTE_3 = "*f3*";

    // Papers
    const char* PAPERS  = "*pap*";

    // All trades
    const char* ALL_TRADES = "*at*";

    // Only MY trades
    const char* MY_TRADES = "*tr*";

    // MY orders
    const char* MY_ORDERS = "*rq*";

    // Balance (position)
    const char* BALANCE = "*bl*";

    // Historical quotes (NOT documented API)
    QRegExp HIST_QUOTES( "\\*ti(\\d+)-(\\d+)\\*\\|\\d+" );
}

/****************************************************************************/

ADConnection::BidOffer::BidOffer () :
    price(0.0),
    buyersQty(0),
    sellersQty(0)
{}

ADConnection::BidOffer::BidOffer ( float p, int b, int s ) :
    price(p),
    buyersQty(b),
    sellersQty(s)
{}

ADConnection::Quote::Quote () :
    paperNo(0),
    lastPrice(0.0)
{}

float ADConnection::Quote::getBestSeller () const
{
    if ( sellers.size() != 0 )
        return sellers.begin().key();
    else
        return 0.0f;
}

float ADConnection::Quote::getBestBuyer () const
{
    if ( buyers.size() != 0 )
        return (buyers.end()-1).key();
    else
        return 0.0f;
}

QString ADConnection::Quote::toStringBestSellers ( quint32 num ) const
{
    if ( num == 0 || sellers.size() == 0 )
        return QString();
    if ( num > (quint32)sellers.size() )
        num = sellers.size();
    QString ret;
    QMap<float, int>::ConstIterator it = sellers.begin();
    for ( quint32 i = 0; i < num; ++i, ++it ) {
        bool lastElem = (i == num - 1);
        ret += QString("%1(%2)%3").arg(it.key()).arg(it.value()).arg(lastElem ? "" : ",");
    }
    return ret;
}

QString ADConnection::Quote::toStringBestBuyers ( quint32 num ) const
{
    if ( num == 0 || buyers.size() == 0 )
        return QString();
    if ( num > (quint32)buyers.size() )
        num = buyers.size();
    QString ret;
    QMap<float, int>::ConstIterator it = buyers.end() - 1;
    for ( quint32 i = 0; i < num && it != buyers.end(); ++i, --it ) {
        bool lastElem = (i == num - 1);
        ret += QString("%1(%2)%3").arg(it.key()).arg(it.value()).arg(lastElem ? "" : ",");
    }
    return ret;
}

ADConnection::HistoricalQuote::HistoricalQuote () :
    paperNo(0),
    open(0.0),
    high(0.0),
    low(0.0),
    close(0.0),
    volume(0.0)
{}

ADConnection::HistoricalQuote::HistoricalQuote (
                                                int paperNo_, float open_, float high_, float low_,
                                                float close_, float volume_, const QDateTime& dt_ ) :
    paperNo(paperNo_),
    open(open_),
    high(high_),
    low(low_),
    close(close_),
    volume(volume_),
    dt(dt_)
{}

ADConnection::Position::Position () :
    paperNo(0),
    qty(0),
    price(0.0),
    varMargin(0.0)
{}

ADConnection::Position::Position ( const QString& accCode_, int paperNo_,
                                   const QString& paperCode_, qint32 qty_,
                                   float price_, float margin_ ) :
    accCode(accCode_),
    paperNo(paperNo_),
    paperCode(paperCode_),
    qty(qty_),
    price(price_),
    varMargin(margin_)
{}

ADConnection::LogParam::LogParam ( bool updateType_, const QDateTime& nowDt_,
                                   const ADFutures& fut_, const ADConnection::Quote& futQuote_,
                                   const ADOption& opt_, const ADConnection::Quote& optQuote_,
                                   float impl_vol_, float sell_impl_vol_, float buy_impl_vol_ ) :
    isFutUpdate(updateType_),
    nowDt(nowDt_),
    fut(fut_),
    futQuote(futQuote_),
    opt(opt_),
    optQuote(optQuote_),
    impl_vol(impl_vol_),
    sell_impl_vol(sell_impl_vol_),
    buy_impl_vol(buy_impl_vol_)
{}

/****************************************************************************/

static ADConnection::Error ADSendHttpsRequest (
    const QString& url,
    const QString& login,
    const QString& passwd,
    QString& outData )
{
    QEventLoop loop;
    QTimer timer;
    QNetworkAccessManager net;
    ADCertificateVerifier certVerifier(login, passwd);

    // Connect some signal staff
    timer.setSingleShot(true);
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    QObject::connect(&net, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));
    QObject::connect(&net,
                     SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
                     &certVerifier,
                     SLOT(onAuthRequired(QNetworkReply*,QAuthenticator*)));
    QObject::connect(&net,
                     SIGNAL(sslErrors(QNetworkReply*,const QList<QSslError>&)),
                     &certVerifier,
                     SLOT(onSslErrors(QNetworkReply*,const QList<QSslError>&)));

    QNetworkRequest request = QNetworkRequest(url);

    // Set user agent like real AD client does
    request.setRawHeader("User-Agent", "ADLite56953");

    QNetworkReply* reply = net.get( request );

    QSslConfiguration conf = reply->sslConfiguration();

    QList<QSslCertificate> certs = QSslCertificate::fromPath(":cacert.pem", QSsl::Pem);
    if ( ! certs.size() ) {
        qWarning("Error: can't find CA certificate for alfadirect authorization!");
        return ADConnection::SSLCertificateError;
    }
    conf.setCaCertificates( certs );
    conf.setProtocol( QSsl::AnyProtocol );
    reply->setSslConfiguration( conf );

    // Start network loop with 5 secs timeout
    timer.start(5000);
    loop.exec();

    // We can call this before real access to reply members,
    // because we are absolutely sure that we are in main thread.
    reply->deleteLater();

    QString replyData;

    // Check that request has been completed
    if ( reply->isFinished() && reply->error() == QNetworkReply::NoError ) {
        timer.stop();

        QTextCodec* codec = QTextCodec::codecForName("Windows-1251");
        if ( ! codec ) {
            qWarning("Can't find codec WIN-1251");
            return ADConnection::SocketError;
        }

        replyData = codec->toUnicode( reply->readAll() );
    }
    // HTTP error
    else if ( reply->isFinished() && reply->error() != QNetworkReply::NoError ) {
        qWarning("HTTP ERROR: some error occured %d", reply->error());
        if ( reply->error() == QNetworkReply::SslHandshakeFailedError )
            return ADConnection::SSLSocketError;
        else
            return ADConnection::SocketError;
    }
    // Timeout
    else {
        qWarning("HTTP ERROR: timeout while sending http request!");
        return ADConnection::SocketError;
    }

    //
    // Success
    //

    // Verify peer certificate second time
    // firstly it should be verified in auth request slot
    if ( ! certVerifier.verifyADCert() ) {
        qWarning("Error: peer is not AlfaDirect! Certificate is counterfeit!");
        return ADConnection::SSLCertificateError;
    }

    outData = replyData;
    return ADConnection::NoError;
}

/****************************************************************************/

TcpReceiver::TcpReceiver ( ADConnection* conn, QTcpSocket& sock ) :
    m_conn(conn),
    m_sock(sock)
{}

void TcpReceiver::tcpReadyRead ()
{
    m_conn->tcpReadyRead( m_sock );
}

void TcpReceiver::tcpError ( QAbstractSocket::SocketError err )
{
    m_conn->tcpError( m_sock, err );
}

void TcpReceiver::tcpStateChanged ( QAbstractSocket::SocketState st )
{
    m_conn->tcpStateChanged( m_sock, st );
}

void TcpReceiver::tcpWriteToSock ( QByteArray data )
{
    bool ret = false;
    m_conn->tcpWriteToSock( data, ret );
}

void TcpReceiver::pingTimer ()
{
    m_conn->sendPing();
}

/****************************************************************************/

SQLReceiver::SQLReceiver ( ADConnection* conn ) :
    m_conn(conn)
{}

void SQLReceiver::sqlFindFutures ( const QString& paperCode,
                                   ADFutures* fut, bool* ret )
{
    Q_ASSERT(fut && ret);
    m_conn->_sqlFindFutures( paperCode, *fut, *ret );
}

void SQLReceiver::sqlFindPaperNo ( const QString& paperCode,
                                   bool usingArchive,
                                   int* paperNo,
                                   bool* ret )
{
    Q_ASSERT(paperNo && ret);
    m_conn->_sqlFindPaperNo( paperCode, usingArchive, *paperNo, *ret );
}

void SQLReceiver::sqlLogQuote ( ADSmartPtr<ADConnection::LogParam> l )
{
    Q_ASSERT(l.isValid());
    m_conn->sqlLogQuote( l->isFutUpdate, l->nowDt, l->fut, l->futQuote, l->opt,
                         l->optQuote, l->impl_vol, l->sell_impl_vol, l->buy_impl_vol );
}

/****************************************************************************/

GenericReceiver::GenericReceiver ( class ADConnection* conn ) :
    m_conn(conn)
{}

void GenericReceiver::onTradePaper ( ADConnection::Order::Operation op )
{
    m_conn->tradePaper( op );
}

void GenericReceiver::onCancelOrder ( ADConnection::Order::Operation op )
{
    m_conn->cancelOrder( op );
}

void GenericReceiver::onChangeOrder ( ADConnection::Order::Operation op,
                                      quint32 pos, float price )
{
    m_conn->changeOrder( op, pos, price );
}

/****************************************************************************/

ADCertificateVerifier::ADCertificateVerifier ( const QString& login,
                                               const QString& passwd ) :
    m_login(login),
    m_passwd(passwd),
    m_peerCertVerified(false)
{}

bool ADCertificateVerifier::verifyADCert () const
{
// Arrrrggghhh!
// On Qt 4.8 peer certificate is null.
// Seems to me this is a bug.
// So, on Qt 4.8 we hope, that nobody will try sign man-in-the-middle server
// by valid CA certificate.
#if QT_VERSION < 0x040800
    if ( !m_peerCertVerified &&
         (!m_peerCert.isValid() ||
          m_peerCert.subjectInfo(QSslCertificate::Organization) != "OAO Alfa-Bank" ||
          m_peerCert.subjectInfo(QSslCertificate::CommonName) != "www.alfadirect.ru" ||
          m_peerCert.subjectInfo(QSslCertificate::LocalityName) != "Moscow" ||
          m_peerCert.subjectInfo(QSslCertificate::OrganizationalUnitName) != "Alfa-Direct" ||
          m_peerCert.subjectInfo(QSslCertificate::CountryName) != "RU" ||
          m_peerCert.subjectInfo(QSslCertificate::StateOrProvinceName) != "Moscow") ) {

        qWarning("Error: peer is not AlfaDirect! Certificate is counterfeit!");
        return false;
    }
#endif

    // Mark as verified
    m_peerCertVerified = true;

    return true;
}

void ADCertificateVerifier::setPeerCert ( const QSslCertificate& cert )
{
    // Drop verification for new cert
    m_peerCertVerified = false;
    m_peerCert = cert;
}

void ADCertificateVerifier::onAuthRequired ( QNetworkReply* reply,
                                             QAuthenticator* auth )
{
    setPeerCert( reply->sslConfiguration().peerCertificate() );
    if ( ! verifyADCert() ) {
        qWarning("Error: peer is not AlfaDirect! Certificate is counterfeit! "
                 "Will not send credentials to peer, just close connection!" );
        return;
    }

    auth->setUser( m_login );
    auth->setPassword( m_passwd );
}

void ADCertificateVerifier::onSslErrors ( QNetworkReply*,
                                          const QList<QSslError>& list )
{
    foreach ( const QSslError& err, list )
        qWarning("SSL ERROR: %s", qPrintable(err.errorString()));
}

/****************************************************************************/

RequestDataPrivate::RequestDataPrivate () :
    m_reqId(ADConnection::InvalidRequestId),
    m_state(ADConnection::Request::RequestUnknown)
{}

void RequestDataPrivate::setState ( ADConnection::Request::State state )
{
    QMutexLocker locker( &m_mutex );
    m_state = state;
    m_wait.wakeAll();
}

void RequestDataPrivate::setRequestId ( ADConnection::RequestId reqId )
{
    QMutexLocker locker( &m_mutex );
    m_reqId = reqId;
}

ADConnection::Request::State RequestDataPrivate::state () const
{
    QMutexLocker locker( &m_mutex );
    return m_state;
}

ADConnection::RequestId RequestDataPrivate::requestId () const
{
    QMutexLocker locker( &m_mutex );
    return m_reqId;
}

bool RequestDataPrivate::wait ( unsigned long timeout )
{
    QMutexLocker locker( &m_mutex );

    if ( m_state != ADConnection::Request::RequestUnknown &&
         m_state != ADConnection::Request::RequestInProgress )
        return true;

    return m_wait.wait( &m_mutex, timeout );
}

/****************************************************************************/

ADConnection::RequestId ADConnection::Request::requestId () const
{
    if ( m_reqData )
        return m_reqData->requestId();
    return ADConnection::InvalidRequestId;
}

ADConnection::Request::State ADConnection::Request::state () const
{
    if ( m_reqData )
        return m_reqData->state();
    return ADConnection::Request::RequestUnknown;
}

bool ADConnection::Request::wait ( unsigned long timeout /* = ULONG_MAX */ )
{
    if ( m_reqData )
        return m_reqData->wait(timeout);
    return false;
}

/****************************************************************************/

quint64 ADConnection::msecsFromEpoch ()
{
#ifdef _WIN_
    quint64 msecs = 0;
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    msecs |= ft.dwHighDateTime;
    msecs <<= 32;
    msecs |= ft.dwLowDateTime;
    // Convert file time to Unix epoch
    msecs /= 10000; // to msecs
    msecs -= 11644473600000ULL; // diff between 1601 and 1970
    return msecs;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((quint64)tv.tv_sec * 1000) + ((quint64)tv.tv_usec / 1000));
#endif
}

void ADConnection::timeMark ( ADConnection::TimeMark& tm )
{
#ifdef _WIN_
    tm = ::GetTickCount();
#elif _LIN_
    timespec ts = {0, 0};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    tm = (((quint64)ts.tv_sec * 1000) + ((quint64)ts.tv_nsec / 1000000));
#elif _MAC_
    tm = ::mach_absolute_time();
#else
#error Unsupported platform
#endif
}

quint32 ADConnection::msecsDiffTimeMark ( const ADConnection::TimeMark& tm1,
                                          const ADConnection::TimeMark& tm2 )
{
#ifdef _WIN_
    // Overflow check, see MSDN of ::GetTickCount for details.
    if ( tm1 > tm2 )
        return (UINT_MAX - (quint32)tm1) + tm2;
    return tm2 - tm1;
#elif _LIN_
    if ( tm1 > tm2 )
        return 0;
    return tm2 - tm1;
#elif _MAC_
    if ( tm1 > tm2 )
        return 0;
    static mach_timebase_info_data_t info = {0, 0};
    if ( info.denom == 0 )
        ::mach_timebase_info(&info);
    return ((tm2 - tm1) * (info.numer / info.denom)) / 1000000;
#else
#error Unsupported platform
#endif
}

ADConnection::TimeMark ADConnection::timeMarkAppendMsecs (
                                                          const ADConnection::TimeMark& tm,
                                                          quint32 msecs )
{
#ifdef _MAC_
    static mach_timebase_info_data_t info = {0, 0};
    if ( info.denom == 0 )
        ::mach_timebase_info(&info);
    return tm + (msecs * 1000000 / (info.numer / info.denom));
#else
    return tm + msecs;
#endif
}

bool ADConnection::stringToTimeFrame ( const QString& str, TimeFrame& ti )
{
    if ( str.compare("1min", Qt::CaseInsensitive) == 0 )
        ti = MIN_1;
    else if ( str.compare("5min", Qt::CaseInsensitive) == 0 )
        ti = MIN_5;
    else if ( str.compare("10min", Qt::CaseInsensitive) == 0 )
        ti = MIN_10;
    else if ( str.compare("15min", Qt::CaseInsensitive) == 0 )
        ti = MIN_15;
    else if ( str.compare("30min", Qt::CaseInsensitive) == 0 )
        ti = MIN_30;
    else if ( str.compare("60min", Qt::CaseInsensitive) == 0 )
        ti = MIN_60;
    else if ( str.compare("day", Qt::CaseInsensitive) == 0 )
        ti = DAY;
    else if ( str.compare("week", Qt::CaseInsensitive) == 0 )
        ti = WEEK;
    else if ( str.compare("month", Qt::CaseInsensitive) == 0 )
        ti = MONTH;
    else
        return false;

    return true;
}

bool ADConnection::ADTimeToDateTime ( int lastUpdate, QDateTime& dt )
{
    static const QDateTime dtFrom =
        QDateTime::fromString( "01.01.1999 00:00:00", "dd.MM.yyyy hh:mm:ss" );
    if ( lastUpdate == 0 )
        return false;
    dt = dtFrom.addSecs( lastUpdate );
    return true;
}

bool ADConnection::dateTimeToADTime ( const QDateTime& dt, int& adTime )
{
    static const QDateTime dtFrom =
        QDateTime::fromString( "01.01.1999 00:00:00", "dd.MM.yyyy hh:mm:ss" );
    if ( ! dt.isValid() || dt < dtFrom )
        return false;
    adTime = dt.toTime_t() - dtFrom.toTime_t();
    return true;
}

/****************************************************************************/

ADConnection::ADConnection () :
    m_lastError(NoError),
    m_state(DisconnectedState),
    m_win1251Codec(0),
#ifdef _WIN_
    m_adLib( ADSmartPtr<ADLibrary>(new ADLocalLibrary) ),
#else
    m_adLib( ADSmartPtr<ADLibrary>(new ADRemoteLibrary) ),
#endif
    m_sock(0),
    m_authed(false),
    m_resendFilters(false),
    // Statistics
    m_statRxNet(0),
    m_statTxNet(0),
    m_statRxDecoded(0),
    m_statTxEncoded(0),
    // Data
    m_ordersOperations( new QHash<RequestId, OrderOpWithPhase> ),
    m_requests( new QHash<RequestId, ADSmartPtr<RequestDataPrivate> > ),
    m_activeOrders( new QHash<Order::OrderId, ADSmartPtr<ADOrderPrivate> > ),
    m_inactiveOrders( new QHash<Order::OrderId, ADSmartPtr<ADOrderPrivate> > ),
    m_reqId(0),
    m_subscriptions( new QList<ADSmartPtr<ADSubscriptionPrivate> > )
{
#ifdef DO_ALL_LOGGING
    m_mainLogFile = ADSmartPtr<QFile>( new QFile(QDir::currentPath() + "/dump.log") );
    if ( ! m_mainLogFile->open(QIODevice::WriteOnly | QIODevice::Append) ) {
        qWarning("Can't open Log file for writing!");
        m_mainLogFile = ADSmartPtr<QFile>();
    }
#endif

    qRegisterMetaType< ADConnection::DataBlock >( "ADConnection::DataBlock" );
    qRegisterMetaType< ADConnection::State >( "ADConnection::State" );
    qRegisterMetaType< ADConnection::Subscription::Type >( "ADConnection::Subscription::Type" );
    qRegisterMetaType< ADConnection::Order::OrderId >( "ADConnection::Order::OrderId" );
    qRegisterMetaType< ADConnection::RequestId >( "ADConnection::RequestId" );
    qRegisterMetaType< ADConnection::Request >( "ADConnection::Request" );
    qRegisterMetaType< ADConnection::Request::State >( "ADConnection::Request::State" );
    qRegisterMetaType< ADConnection::Order >( "ADConnection::Order" );
    qRegisterMetaType< ADConnection::Order::Type >( "ADConnection::Order::Type" );
    qRegisterMetaType< ADConnection::Order::State >( "ADConnection::Order::State" );
    qRegisterMetaType< ADConnection::Order::Operation >( "ADConnection::Order::Operation" );
    qRegisterMetaType< ADConnection::Order::OperationResult >( "ADConnection::Order::OperationResult" );
    qRegisterMetaType< ADConnection::Order::OperationType >( "ADConnection::Order::OperationType" );
    qRegisterMetaType< ADSmartPtr<ADConnection::LogParam> >( "ADSmartPtr<ADConnection::LogParam>" );
    qRegisterMetaType< QVector<ADConnection::HistoricalQuote> >( "QVector<ADConnection::HistoricalQuote>" );
}

ADConnection::~ADConnection ()
{
    disconnect();

    // Drop subscriptions
    QList<ADSmartPtr<ADSubscriptionPrivate> >::Iterator itSub = m_subscriptions->begin();
    for ( ; itSub != m_subscriptions->end(); ++itSub ) {
        if ( (*itSub).countRefs() > 1 ) {
            qWarning("Connection is going to be destructed, but subscription is in use!");
            (*itSub)->zeroConnectionMember();
        }
    }

    delete m_subscriptions;
    delete m_ordersOperations;
    delete m_requests;
    delete m_activeOrders;
    delete m_inactiveOrders;
}

bool ADConnection::connect ( const QString& login, const QString& passwd )
{
    QMutexLocker locker( &m_mutex );

    if ( QThread::isRunning() )
        return false;

    m_login = login;
    m_password = passwd;

    QThread::start();

    if ( ! QThread::isRunning() )
        return false;

    return true;
}

void ADConnection::disconnect ()
{
    QMutexLocker locker( &m_mutex );

    if ( ! QThread::isRunning() )
        return;

    QThread::quit();
    QThread::wait();
}

bool ADConnection::isConnected () const
{
    return (QThread::isRunning() && m_state == ConnectedState);
}

bool ADConnection::serverTime ( QDateTime& srvTime ) const
{
    if ( ! isConnected() )
        return false;

    // Lock
    QReadLocker readLocker( &m_rwLock );
    if ( ! m_srvTime.isValid() || ! m_srvTimeUpdate.isValid())
        return false;

    QDateTime now = QDateTime::currentDateTime();
    int secs = m_srvTimeUpdate.secsTo( now );
    srvTime = m_srvTime.addSecs( secs );
    return true;
}

ADConnection::State ADConnection::state () const
{
    return m_state;
}

ADConnection::Error ADConnection::error () const
{
    return m_lastError;
}

bool ADConnection::getNetworkStatistics (
                                         quint64& rxNet, quint64& txNet,
                                         quint64& rxDecoded, quint64& txEncoded ) const
{
    if ( ! isConnected() )
        return false;

    rxNet = atomic_read64(&m_statRxNet);
    txNet = atomic_read64(&m_statTxNet);
    rxDecoded = atomic_read64(&m_statRxDecoded);
    txEncoded = atomic_read64(&m_statTxEncoded);

    return true;
}

ADConnection::Subscription ADConnection::subscribeToQuotes (
                                                            const QList<ADConnection::Subscription::Options>& opts )
{
    ADConnection::Subscription subscr;

    //Lock
    QWriteLocker wLocker( &m_rwLock );

    QSet<QString> filterKeysForAll;

    // Update filters
    for ( int i = 0; i < opts.size(); ++i ) {
        QSet<QString> filterKeys;
        if ( opts[i].m_subscrTypeReceive & ADConnection::Subscription::QueueSubscription ) {
            filterKeys << "filter_q";
        }
        if ( opts[i].m_subscrTypeReceive & ADConnection::Subscription::QuoteSubscription ) {
            filterKeys << "filter_f";
        }
        QSet<QString>::Iterator it = filterKeys.begin();
        while ( it != filterKeys.end() ) {
            const QString& filterKey = *it;
            QHash<QString, int>& quotesFilter = m_complexFilter[filterKey];
            bool updateKey = false;
            foreach ( int paperNo, opts[i].m_paperNos ) {
                if ( 1 == ++quotesFilter[QString("%1").arg(paperNo)] )
                    updateKey = true;
            }
            // Remove key if it should not be updated
            if ( ! updateKey )
                it = filterKeys.erase(it);
            else
                ++it;
        }
        filterKeysForAll.unite( filterKeys );
    }

    bool sendRes = sendADFilters( QSet<QString>(), m_simpleFilter,
                                  filterKeysForAll, m_complexFilter );
    if ( ! sendRes ) {
        qWarning("Send AD filters failed!");
        return subscr;
    }

    ADSmartPtr<ADSubscriptionPrivate> subscrPtr( new ADSubscriptionPrivate(this, opts) );
    subscr.m_subscr = subscrPtr;

    // Append
    m_subscriptions->append( subscrPtr );

    return subscr;
}

bool ADConnection::_unsubscribeToQuote ( const ADSmartPtr<ADSubscriptionPrivate>& subscr )
{
    if ( ! subscr ) {
        qWarning("Subscription is invalid!");
        return false;
    }

    const QList<ADConnection::Subscription::Options>& opts = subscr->subscriptionOptions();
    QSet<QString> filterKeysForAll;

    // Update filters
    for ( int i = 0; i < opts.size(); ++i ) {
        QSet<QString> filterKeys;
        if ( opts[i].m_subscrTypeReceive & ADConnection::Subscription::QueueSubscription ) {
            filterKeys << "filter_q";
        }
        if ( opts[i].m_subscrTypeReceive & ADConnection::Subscription::QuoteSubscription ) {
            filterKeys << "filter_f";
        }
        QSet<QString>::Iterator it = filterKeys.begin();
        while ( it != filterKeys.end() ) {
            const QString& filterKey = *it;
            QHash<QString, int>& quotesFilter = m_complexFilter[filterKey];
            bool updateKey = false;
            foreach ( int paperNo, opts[i].m_paperNos ) {
                QString key = QString("%1").arg(paperNo);
                if ( quotesFilter.contains(key) && --quotesFilter[ key ] == 0 ) {
                    updateKey = true;
                    quotesFilter.remove(key);
                }
            }
            // Remove key if it should not be updated
            if ( ! updateKey )
                it = filterKeys.erase(it);
            else
                ++it;
        }
        filterKeysForAll.unite( filterKeys );
    }

    return sendADFilters( QSet<QString>(), m_simpleFilter,
                          filterKeysForAll, m_complexFilter );
}

bool ADConnection::requestHistoricalQuotes ( int paperNo,
                                             ADConnection::TimeFrame timeFrame,
                                             const QDateTime& fromDt,
                                             const QDateTime& toDt,
                                             ADConnection::Request& req )
{
    if ( paperNo <= 0 || ! fromDt.isValid() || ! toDt.isValid() ||
         toDt < fromDt )
        return false;

    QString timeFrameStr;
    switch ( timeFrame ) {
    case MIN_1: {
        timeFrameStr = "period=0&interval=1&type=0";
        break;
    }
    case MIN_5: {
        timeFrameStr = "period=0&interval=5&type=1";
        break;
    }
    case MIN_10: {
        timeFrameStr = "period=0&interval=10&type=2";
        break;
    }
    case MIN_15: {
        timeFrameStr = "period=0&interval=15&type=3";
        break;
    }
    case MIN_30: {
        timeFrameStr = "period=0&interval=30&type=4";
        break;
    }
    case MIN_60: {
        timeFrameStr = "period=0&interval=60&type=5";
        break;
    }
    case DAY: {
        timeFrameStr = "period=1&type=6";
        break;
    }
    case WEEK: {
        timeFrameStr = "period=2&type=7";
        break;
    }
    case MONTH: {
        timeFrameStr = "period=3&type=8";
        break;
    }
    default:
        Q_ASSERT(0);
        return false;
    }

    ADSmartPtr<RequestDataPrivate> reqData( new(std::nothrow) RequestDataPrivate );
    if ( ! reqData.isValid() ) {
        qWarning("Allocation problems!");
        return false;
    }

    // Lock
    QWriteLocker wLocker( &m_rwLock );

    // Iterate request
    ++m_reqId;

    reqData->setRequestId(m_reqId);
    req.m_reqData = reqData;
    m_requests->insert(m_reqId, reqData);

    QString cmd = QString("id=%1|ChartDataRequest\r\n"
                          "paper_no=%2&%3&from_date=%4&to_date=%5\r\n\r\n")
        .arg(m_reqId)
        .arg(paperNo)
        .arg(timeFrameStr)
        .arg(fromDt.toString("yyyy-MM-dd hh:ss"))
        .arg(toDt.toString("yyyy-MM-dd hh:ss"));

    // Unlock
    wLocker.unlock();

    // Write to server in latin1
    bool res = writeToSock( cmd.toLatin1() );
    if ( ! res ) {
        m_requests->remove(m_reqId);
    }

    return res;
}

bool ADConnection::_getQuote ( int paperNo, ADConnection::Quote& quote ) const
{
    if ( ! m_quotes.contains(paperNo) )
        return false;
    quote = m_quotes[paperNo];
    return true;
}

bool ADConnection::_findOperationsByOrderId ( ADConnection::Order::OrderId orderId,
                                              QList< ADSmartPtr<ADOrderOperationPrivate> >& list ) const
{
    list.clear();
    bool ret = false;
    QHash<RequestId, OrderOpWithPhase>::Iterator it = m_ordersOperations->begin();
    for ( ; it != m_ordersOperations->end(); ++it ) {
        ADSmartPtr<ADOrderOperationPrivate>& op = it.value().first;
        if ( op->getOrder()->getOrderId() == orderId ) {
            list.append(op);
            ret = true;
        }
    }
    return ret;
}

bool ADConnection::getQuote ( int paperNo, ADConnection::Quote& quote ) const
{
    // Lock
    QReadLocker readLocker( &m_rwLock );
    return _getQuote(paperNo, quote);
}

bool ADConnection::getPosition ( const QString& acc,
                                 const QString& paperCode,
                                 Position& pos ) const
{
    // Lock
    QReadLocker readLocker( &m_rwLock );
    if ( ! m_positions.contains(acc) || ! m_positions[acc].contains(paperCode) )
        return false;
    pos = m_positions[acc][paperCode];
    return true;
}

bool ADConnection::findFutures ( const QString& futCode, ADFutures& fut )
{
    bool res = false;
    if ( QThread::currentThread() == this ) {
        _sqlFindFutures(futCode, fut, res);
        return res;
    }
    else {
        emit onFindFutures(futCode, &fut, &res);
        return res;
    }
}

void ADConnection::_sqlFindFutures ( const QString& futCode,
                                     ADFutures& fut,
                                     bool& found )
{
    Q_ASSERT(QThread::currentThread() == this);
    found = false;

    if ( ! m_adDB.isOpen() ) {
        qWarning("Warning: can't find futures, DB is closed!");
        return;
    }

    fut = ADFutures();
    fut.paperCode = futCode;
    fut.paperNo = 0;

    QSqlQuery query( m_adDB );
    const QString sql =
        "SELECT papers.\"paper_no\", "
        "                 papers.\"p_code\", "
        "                 papers.\"mat_date\", "
        "                 actives.\"strike\", "
        "                 actives.\"at_code\" "
        "FROM (SELECT \"paper_no\" "
        "                       FROM AD_PAPERS WHERE \"p_code\" = :p_code) AS future, "
        "               AD_ACTIVES AS actives, "
        "               AD_PAPERS AS papers "
        "WHERE "
        "               (papers.\"paper_no\" = future.\"paper_no\" OR "
        "                papers.\"base_paper_no\" = future.\"paper_no\") AND "
        "                papers.\"p_code\" = actives.\"p_code\" AND "
        "                papers.\"expired\" = 'N' AND "
        "               (actives.\"at_code\" like 'F%' OR "
        "                actives.\"at_code\" = 'OC' OR "
        "                actives.\"at_code\" = 'OP' OR "
        "                actives.\"at_code\" = 'OCM' OR "
        "                actives.\"at_code\" = 'OPM') AND "
        "                papers.\"mat_date\" >= CURRENT_TIMESTAMP "
        "ORDER BY "
        "                papers.\"mat_date\" DESC, "
        "                actives.\"strike\", "
        "                actives.\"at_code\" ";

    // Execute query
    query.prepare( sql );
    query.bindValue(":p_code", futCode);
    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
        found = false;
        return;
    }

    // Fetch result
    while ( query.next() ) {
        int paperNo = query.value(0).toInt();
        QString paperCode = query.value(1).toString();
        QDate matDate = query.value(2).toDate();
        float strike = query.value(3).toDouble();
        QString paperType = query.value(4).toString();

        ADOption::Type t = ADOption::Invalid;
        ADOption::PriceType priceType = ADOption::Stock;

        if ( paperType.startsWith("F") ) {
            fut.paperNo = paperNo;
            fut.paperCode = paperCode;
            continue;
        }
        else if ( paperType == "OC" ) {
            t = ADOption::Call;
            priceType = ADOption::Stock;
        }
        else if ( paperType == "OCM" ) {
            t = ADOption::Call;
            priceType = ADOption::Margin;
        }
        else if ( paperType == "OP" ) {
            t = ADOption::Put;
            priceType = ADOption::Stock;
        }
        else if ( paperType == "OPM" ) {
            t = ADOption::Put;
            priceType = ADOption::Margin;
        }
        else {
            qWarning("Unknown paper type: %s\n", qPrintable(paperType));
            Q_ASSERT(0);
            continue;
        }

        ADOption opt( paperCode, paperNo, fut.paperNo, matDate, strike, t, priceType );

        // Append to plain map
        fut.optionsPlainMap[paperCode] = opt;

        // Append to complex map
        QMap<ADOption::PriceType, ADOptionPair>& optPairMap = fut.options[matDate][strike];
        ADOptionPair& optPair = optPairMap[priceType];
        if ( opt.type == ADOption::Call ) {
            if ( optPair.optionCall.type != ADOption::Invalid )
                qWarning("Warning: option exists (was '%s', new '%s')",
                         qPrintable(optPair.optionCall.paperCode),
                         qPrintable(opt.paperCode));
            optPair.optionCall = opt;
        }
        else {
            if ( optPair.optionPut.type != ADOption::Invalid )
                qWarning("Warning: option exists (was '%s', new '%s')",
                         qPrintable(optPair.optionPut.paperCode),
                         qPrintable(opt.paperCode));
            optPair.optionPut = opt;
        }
        optPair.priceType = priceType;
        optPair.strike = strike;
        optPair.matDate = matDate;
    }

    // Remove not full pairs
    {
        // Iterate over options' maturity dates
        for ( QMap<QDate,
                  QMap<float,
                  QMap<ADOption::PriceType, ADOptionPair> > >::Iterator optMapIt =
                  fut.options.begin();
              optMapIt != fut.options.end(); ) {
            // Iterate over options' price types
            for ( QMap<float,
                      QMap<ADOption::PriceType, ADOptionPair> >::Iterator optPrMapIt =
                      optMapIt->begin();
                  optPrMapIt != optMapIt->end(); ) {
                // Iterate over all options
                for ( QMap<ADOption::PriceType, ADOptionPair>::Iterator optIt = optPrMapIt->begin();
                      optIt != optPrMapIt->end(); ) {

                    ADOptionPair& optPair = optIt.value();

                    if ( optPair.optionCall.type == ADOption::Invalid ||
                         optPair.optionPut.type == ADOption::Invalid ) {
                        qWarning("Not full pair: call=('%s', type='%s', "
                                 "invalid=%d), put=('%s', type='%s', invalid=%d)",
                                 qPrintable(optPair.optionCall.paperCode), "call",
                                 optPair.optionCall.type == ADOption::Invalid,
                                 qPrintable(optPair.optionPut.paperCode), "put",
                                 optPair.optionPut.type == ADOption::Invalid);
                        optIt = optPrMapIt->erase( optIt );
                    }
                    else
                        ++optIt;
                }
                /////////////////
                if ( optPrMapIt->size() == 0 )
                    optPrMapIt = optMapIt->erase( optPrMapIt );
                else
                    ++optPrMapIt;
                /////////////////
            }
            /////////////////
            if ( optMapIt->size() == 0 )
                optMapIt = fut.options.erase( optMapIt );
            else
                ++optMapIt;
            /////////////////
        }
    }

    found = (fut.paperNo != 0);
}

bool ADConnection::findPaperNo ( const QString& papCode,
                                 bool usingArchive,
                                 int& paperNo )
{
    bool res = false;
    if ( QThread::currentThread() == this ) {
        _sqlFindPaperNo(papCode, usingArchive, paperNo, res);
        return res;
    }
    else {
        emit onFindPaperNo(papCode, usingArchive, &paperNo, &res);
        return res;
    }
}

void ADConnection::_sqlFindPaperNo ( const QString& papCode,
                                     bool usingArchive,
                                     int& paperNo,
                                     bool& found )
{
    Q_ASSERT(QThread::currentThread() == this);
    paperNo = 0;
    found = false;

    // Check paper code is valid
    if ( papCode.isEmpty() )
        return;

    if ( ! m_adDB.isOpen() ) {
        qWarning("Warning: can't find paper, DB is closed!");
        return;
    }

    QSqlQuery query( m_adDB );
    const QString sql =
        " SELECT \"paper_no\" FROM AD_PAPERS AS papers "
        "    WHERE \"p_code\" LIKE ? "
        "      UNION "
        " SELECT \"paper_no\" FROM AD_ARCHIVE_PAPERS AS arch_papers "
        "    WHERE \"p_code\" LIKE ? ";

    // Execute query
    query.prepare( sql );
    query.addBindValue(papCode);
    query.addBindValue(papCode);
    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
        found = false;
        return;
    }

    // Fetch result
    while ( query.next() ) {
        paperNo = query.value(0).toInt();
        break;
    }

    if ( paperNo != 0 ) {
        found = true;
        return;
    }
    // Try to make archive request to AD
    else if ( usingArchive ) {
        QString url = QString("https://www.alfadirect.ru/ads/find_archive_papers.idc?text=%1").arg(papCode);

        // Send https request
        QString response;
        Error err = ADSendHttpsRequest(url, m_login, m_password, response);
        if ( err != NoError )
            return;

        // Parse reply data
        QStringList lines = response.split("\n", QString::SkipEmptyParts);
        for ( QStringList::Iterator it = lines.begin();
              it != lines.end(); ++it ) {
            QString& line = *it;
            QStringList cols = line.split("|");

            bool ok = (cols.size() >= 10);
            if ( ! ok ) {
                qWarning("Wrong archive papers line: can't parse line!");
                continue;
            }

            int realPapNo = cols[0].toInt(&ok);
            if ( ! ok ) {
                qWarning("Wrong archive papers line: can't parse paper no!");
                continue;
            }
            QString realPapCode = cols[1];
            QString tsPapCode = cols[2];
            QString placeCode = cols[3];
            QString placeName = cols[4];
            int unused = cols[5].toInt(&ok);
            if ( ! ok ) {
                qWarning("Wrong archive papers line: can't parse unused!");
                continue;
            }

            QString expired = cols[6];
            QString boardCode = cols[7];
            QString atCode = cols[8];
            QDateTime matDate = QDateTime::fromString( cols[9], "dd/MM/yyyy" );
            if ( ! ok ) {
                qWarning("Wrong archive papers line: can't parse maturity date!");
                continue;
            }

            sqlInsertArchivePaper( realPapNo, realPapCode, tsPapCode,
                                   placeCode, placeName, unused, expired,
                                   boardCode, atCode, matDate );

            if ( ! found && papCode == realPapCode ) {
                paperNo = realPapNo;
                found = true;
            }
        }
    }
}

bool ADConnection::_sqlFindActiveOrders ( const QString& accCode,
                                          const QString& paperCode,
                                          QList< ADSmartPtr<ADOrderPrivate> >& orders )
{
    Q_ASSERT(QThread::currentThread() == this);

    if ( ! m_adDB.isOpen() ) {
        qWarning("Warning: can't find active order, DB is closed!");
        return false;
    }

    orders.clear();

    QSqlQuery query( m_adDB );
    const QString sql =
        "SELECT orders.\"ord_no\", "
        "                 orders.\"status\", "
        "                 orders.\"b_s\", "
        "                 orders.\"price\", "
        "                 orders.\"qty\", "
        "                 orders.\"p_code\", "
        "                 orders.\"drop_time\" "
        "FROM AD_ORDERS as orders "
        "WHERE "
        "     orders.\"acc_code\" = :acc_code AND "
        "     orders.\"p_code\" = :p_code AND "
        "               (orders.\"status\" = 'O' OR "
        "      orders.\"status\" = 'X' OR "
        "      orders.\"status\" = 'N') "
        "ORDER BY "
        "                orders.\"ord_no\" ";

    // Execute query
    query.prepare( sql );
    query.bindValue(":acc_code", accCode);
    query.bindValue(":p_code", paperCode);
    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
        return false;
    }

    // Fetch result
    while ( query.next() ) {
        Order::OrderId orderNo = query.value(0).toInt();
        QString status = query.value(1).toString();
        QString b_s = query.value(2).toString();
        float price = query.value(3).toDouble();
        int qty = query.value(4).toInt();
        QString paperCode = query.value(5).toString();
        QDateTime dropDt = query.value(6).toDateTime();

        Order::Type orderBS = (b_s == "S" ? Order::Sell : Order::Buy);
        ADSmartPtr<ADOrderPrivate> order( new ADOrderPrivate(accCode, orderBS, paperCode, qty, price, dropDt, orderNo) );
        if ( order.isValid() )
            orders.append(order);
    }
    return (orders.size() > 0);
}

bool ADConnection::_sqlGetCurrentPosition ( const QString& accCode,
                                            const QString& paperCode,
                                            ADConnection::Position& position )
{
    Q_ASSERT(QThread::currentThread() == this);

    bool found = false;

    QSqlQuery query( m_adDB );
    const QString sql =
        "SELECT pos.\"acc_code\", "
        "                 pos.\"p_code\", "
        "                 pos.\"paper_no\", "
        "                 pos.\"real_rest\", "
        "                 pos.\"balance_price\", "
        "                 pos.\"var_margin\" "
        "FROM AD_BALANCE as pos "
        "WHERE "
        "     pos.\"acc_code\" = :acc_code AND "
        "     pos.\"p_code\" = :p_code ";

    // Execute query
    query.prepare( sql );
    query.bindValue(":acc_code", accCode);
    query.bindValue(":p_code", paperCode);
    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
        return false;
    }

    // Fetch result
    while ( query.next() ) {
        QString accCode = query.value(0).toString();
        QString paperCode = query.value(1).toString();
        int paperNo = query.value(2).toInt();
        float realRest = query.value(3).toDouble();
        float balancePrice = query.value(4).toDouble();
        float varMargin = query.value(5).toDouble();

        position = Position(accCode, paperNo, paperCode, static_cast<int>(realRest),
                            balancePrice, varMargin);

        found = true;
        break;
    }
    return found;
}

bool ADConnection::_sqlGetDBSchema ( QHash<QString, QStringList>& schema )
{
    Q_ASSERT(QThread::currentThread() == this);

    schema.clear();

    QStringList tables = m_adDB.tables( QSql::Tables );
    foreach ( QString tableName, tables ) {
        QSqlRecord fieldsRec = m_adDB.record( tableName );
        for ( int i = 0; i < fieldsRec.count(); ++i ) {
            QString fieldName = fieldsRec.fieldName(i);

            tableName.remove(QRegExp("\\s+$"));
            fieldName.remove(QRegExp("\\s+$"));
            schema[tableName].append(fieldName);
        }
    }

    return (schema.size() > 0);
}

bool ADConnection::_sqlExecSelect (
    const QString& tableName,
    const QMap<QString, QVariant>& search,
    QSqlQuery& resQuery ) const
{
    Q_ASSERT(QThread::currentThread() == this );

    QStringList keys = search.keys();

    QStringList where;
    for ( int i = 0; i < keys.size(); ++i ) {
        where.append(QString("t.\"%1\" = :%2").arg(keys[i]).arg(i));
    }

    QString sql = QString("SELECT * FROM \"%1\" AS t WHERE %2").
        arg(tableName).
        arg(where.join(" AND "));

    QSqlQuery query( m_adDB );
    query.prepare( sql );
    foreach ( const QString& key, keys ) {
        query.addBindValue(search[key]);
    }

    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
        return false;
    }

    resQuery = query;
    return true;
}

void ADConnection::lockForRead ()
{
    m_rwLock.lockForRead();
}

void ADConnection::unlock ()
{
    m_rwLock.unlock();
}

ADConnection::Order::Operation ADConnection::changeOrder ( ADConnection::Order order,
                                                           quint32 qty, float price )
{
    if ( ! order.m_order.isValid() )
        return ADConnection::Order::Operation();

    // Lock
    QWriteLocker wLocker( &m_rwLock );
    // Check active orders
    if ( ! m_activeOrders->contains(order.m_order->getOrderId()) )
        return ADConnection::Order::Operation();

    QList< ADSmartPtr<ADOrderOperationPrivate> > ordersOps;
    if ( _findOperationsByOrderId(order.m_order->getOrderId(), ordersOps) ) {
        // Maybe sometime will analyse orders operations list! Not now
        qWarning("There are some pending order operations!");
        return ADConnection::Order::Operation();
    }

    // Iterate request
    RequestId reqId = ++m_reqId;

    // Create order operation
    ADSmartPtr<ADOrderOperationPrivate> op(
                                         new ADOrderOperationPrivate(reqId, ADConnection::Order::ChangeOrder, order.m_order) );

    // Save drop request
    m_ordersOperations->insert(reqId, OrderOpWithPhase(op, 0));

    // Change status to cancelling
    Order::State oldState = order.m_order->getOrderState();
    order.m_order->setOrderState( Order::ChangingState );

    // Unlock
    wLocker.unlock();

    emit onOrderStateChanged( ADConnection::Order(order.m_order),
                              oldState,
                              Order::CancellingState );

    ADConnection::Order::Operation orderOp(op);

    if ( QThread::currentThread() == this ) {
        changeOrder(orderOp, qty, price);
    }
    else {
        emit onChangeOrder(orderOp, qty, price);
    }

    return orderOp;
}

void ADConnection::changeOrder ( ADConnection::Order::Operation op,
                                 quint32 newQty, float newPrice )
{
    Q_ASSERT(op.isValid());
    ADSmartPtr<ADOrderPrivate> order = op.m_op->getOrder();
    Q_ASSERT(order.isValid());
    ADConnection::RequestId reqId = op.m_op->getRequestId();

    const QString editOrderName("edit_order");
    QByteArray editOrderDoc;
    //XXX TURN OFF CACHE FOR NOW!
    if ( 0 && ! getCachedTemplateDocument(editOrderName, editOrderDoc) ) {
        qWarning("Error: can't get template '%s'!", qPrintable(editOrderName));

        // Lock
        QWriteLocker wLocker( &m_rwLock );
        Q_ASSERT(m_ordersOperations->contains(reqId));
        OrderOpWithPhase orderPhasePtr = m_ordersOperations->take(reqId);
        // Back to accepted
        Order::State oldState = order->getOrderState();
        order->setOrderState( Order::AcceptedState );
        op.m_op->wakeUpAll( Order::ErrorResult );
        // Unlock
        wLocker.unlock();

        emit onOrderStateChanged( ADConnection::Order(order),
                                  oldState,
                                  Order::AcceptedState );
        emit onOrderOperationResult( ADConnection::Order(order),
                                     op,
                                     Order::ErrorResult );
        return;
    }

    //
    // XXX Create template manually!
    //

    QString editOrderDocStr = "XXX: TODO";

    QDateTime now = QDateTime::currentDateTime();
    QString signDt = now.toString("dd/MM/yyyy hh:mm");
    QString dropDt = order->getOrderDropDateTime().toString("dd/MM/yyyy");
    QString dropDtHHMM = order->getOrderDropDateTime().toString("dd/MM/yyyy hh:mm");
    QString newDropDt = dropDt;
    QString operationStr = (order->getOrderType() == Order::Buy ?
                            QString::fromUtf8("купить") :
                            QString::fromUtf8("продать"));
    QString operationCh = (order->getOrderType() == Order::Buy ? "B" : "S");
    int priceInt = (int)order->getOrderPrice();
    int newPriceInt = (int)newPrice;
    QString XXX_portfolio = "XXX-0000";

    editOrderDoc = editOrderDocStr.
        arg(XXX_portfolio).
        arg(order->getOrderId()).
        arg(operationStr).
        arg(order->getOrderQty()).
        arg(priceInt).
        arg(dropDt).
        arg(newQty).
        arg(newPriceInt).
        arg(newDropDt).
        arg(signDt).
        toLatin1();

    //
    // Make signature
    //
    unsigned int size = 0;
    char* data = 0;
    bool signRes = m_adLib->makeSignature(m_sessInfo.provCtx, m_sessInfo.certCtx,
                                          editOrderDoc.data(), editOrderDoc.size(),
                                          &data, &size);
    if ( ! signRes || data == 0 || size == 0 ) {
        qWarning("Error while sign!");
        // Lock
        QWriteLocker wLocker( &m_rwLock );
        Q_ASSERT(m_ordersOperations->contains(reqId));
        OrderOpWithPhase orderPhasePtr = m_ordersOperations->take(reqId);
        // Back to accepted
        Order::State oldState = order->getOrderState();
        order->setOrderState( Order::AcceptedState );
        op.m_op->wakeUpAll( Order::ErrorResult );
        // Unlock
        wLocker.unlock();
        emit onOrderStateChanged( ADConnection::Order(order),
                                  oldState,
                                  Order::AcceptedState );
        emit onOrderOperationResult( ADConnection::Order(order),
                                     op,
                                     Order::ErrorResult );

        return;
    }
    QByteArray sign( data, size );
    m_adLib->freeMemory( data );

    //
    // Create url
    //
    //XXXX
    QString cmd("id=%1|ChangeOrder\r\n"
                "ord_no=%2&blank=L&b_s=%3&acc_code=%4&place_code=FORTS&limits_check=Y&"
                "p_code=RTSI-3.10&price_currency=RUR&ch_drop_time=%5&new_price=%6"
                "new_paper_qty=%7&new_end_date=%8&old_price=%9&old_qty=%10&old_end_date=%11&"
                "sign=%12&sign_time=%13&is_new_template=Y\r\n\r\n"
                );
    cmd = cmd.
        arg(reqId).
        arg(order->getOrderId()).
        arg(operationCh).
        arg(XXX_portfolio).
        arg(dropDtHHMM).
        arg(newPriceInt).
        arg(newQty).
        arg(newDropDt).
        arg(priceInt).
        arg(order->getOrderQty()).
        arg(dropDt).
        arg(sign.data()).
        arg(signDt)
        ;

    //
    // Send
    //
    bool res = writeToSock( cmd.toLatin1() );
    if ( ! res ) {
        qWarning("send failed!");
        // Lock
        QWriteLocker wLocker( &m_rwLock );
        Q_ASSERT(m_ordersOperations->contains(reqId));
        OrderOpWithPhase orderPhasePtr = m_ordersOperations->take(reqId);
        // Back to accepted
        Order::State oldState = order->getOrderState();
        order->setOrderState( Order::AcceptedState );
        op.m_op->wakeUpAll( Order::ErrorResult );
        // Unlock
        wLocker.unlock();
        emit onOrderStateChanged( ADConnection::Order(order),
                                  oldState,
                                  Order::AcceptedState );
        emit onOrderOperationResult( ADConnection::Order(order),
                                     op,
                                     Order::ErrorResult );

        QThread::quit();
        return;
    }
}

ADConnection::Order::Operation ADConnection::cancelOrder ( ADConnection::Order order )
{
    if ( ! order.m_order.isValid() )
        return ADConnection::Order::Operation();

    // Lock
    QWriteLocker wLocker( &m_rwLock );
    // Check active orders
    if ( ! m_activeOrders->contains(order.m_order->getOrderId()) )
        return ADConnection::Order::Operation();

    QList< ADSmartPtr<ADOrderOperationPrivate> > ordersOps;
    if ( _findOperationsByOrderId(order.m_order->getOrderId(), ordersOps) ) {
        // Maybe sometime will analyse orders operations list! Not now
        qWarning("There are some pending order operations!");
        return ADConnection::Order::Operation();
    }

    // Iterate request
    RequestId reqId = ++m_reqId;

    // Create order operation
    ADSmartPtr<ADOrderOperationPrivate> op(
                                         new ADOrderOperationPrivate(reqId, ADConnection::Order::CancelOrder, order.m_order) );

    // Save drop request
    m_ordersOperations->insert(reqId, OrderOpWithPhase(op, 0));

    // Change status to cancelling
    Order::State oldState = order.m_order->getOrderState();
    order.m_order->setOrderState( Order::CancellingState );

    // Unlock
    wLocker.unlock();

    emit onOrderStateChanged( ADConnection::Order(order.m_order),
                              oldState,
                              Order::CancellingState );

    ADConnection::Order::Operation orderOp(op);

    if ( QThread::currentThread() == this ) {
        cancelOrder(orderOp);
    }
    else {
        emit onCancelOrder(orderOp);
    }

    return orderOp;
}

void ADConnection::cancelOrder ( ADConnection::Order::Operation op )
{
    Q_ASSERT(op.isValid());
    ADSmartPtr<ADOrderPrivate> order = op.m_op->getOrder();
    Q_ASSERT(order.isValid());
    ADConnection::RequestId reqId = op.m_op->getRequestId();

    const QString killOrderName("kill_order");
    QByteArray killOrderDoc;
    //XXX TURN OFF CACHE FOR NOW!
    if ( 0 && ! getCachedTemplateDocument(killOrderName, killOrderDoc) ) {
        qWarning("Error: can't get template '%s'!", qPrintable(killOrderName));

        // Lock
        QWriteLocker wLocker( &m_rwLock );
        Q_ASSERT(m_ordersOperations->contains(reqId));
        OrderOpWithPhase orderPhasePtr = m_ordersOperations->take(reqId);
        // Back to accepted
        Order::State oldState = order->getOrderState();
        order->setOrderState( Order::AcceptedState );
        op.m_op->wakeUpAll( Order::ErrorResult );
        // Unlock
        wLocker.unlock();

        emit onOrderStateChanged( ADConnection::Order(order),
                                  oldState,
                                  Order::AcceptedState );
        emit onOrderOperationResult( ADConnection::Order(order),
                                     op,
                                     Order::ErrorResult );

        return;
    }

    //
    // XXX Create template manually!
    //

    QString killOrderDocStr = "XXX: TODO";

    QDateTime now = QDateTime::currentDateTime();
    QString signDt = now.toString("dd/MM/yyyy hh:mm");
    QString operationStr = (order->getOrderType() == Order::Buy ?
                            QString::fromUtf8("Покупка") :
                            QString::fromUtf8("Продажа"));
    int priceInt = (int)order->getOrderPrice();
    QString XXX_portfolio = "XXX-0000";

    killOrderDoc = killOrderDocStr.
        arg(order->getOrderId()).
        arg(XXX_portfolio).
        arg(priceInt).
        arg(order->getOrderQty()).
        arg(operationStr).
        arg(signDt).
        toLatin1();

    //
    // Make signature
    //
    unsigned int size = 0;
    char* data = 0;
    bool signRes = m_adLib->makeSignature(m_sessInfo.provCtx, m_sessInfo.certCtx,
                                          killOrderDoc.data(), killOrderDoc.size(),
                                          &data, &size);
    if ( ! signRes || data == 0 || size == 0 ) {
        qWarning("Error while sign!");
        // Lock
        QWriteLocker wLocker( &m_rwLock );
        Q_ASSERT(m_ordersOperations->contains(reqId));
        OrderOpWithPhase orderPhasePtr = m_ordersOperations->take(reqId);
        // Back to accepted
        Order::State oldState = order->getOrderState();
        order->setOrderState( Order::AcceptedState );
        op.m_op->wakeUpAll( Order::ErrorResult );
        // Unlock
        wLocker.unlock();
        emit onOrderStateChanged( ADConnection::Order(order),
                                  oldState,
                                  Order::AcceptedState );
        emit onOrderOperationResult( ADConnection::Order(order),
                                     op,
                                     Order::ErrorResult );

        return;
    }
    QByteArray sign( data, size );
    m_adLib->freeMemory( data );

    //
    // Create url
    //
    //XXXX
    QString cmd("id=%1|DelOrder\r\n"
                "ord_no=%2&sign=%3&sign_time=%4&is_new_template=Y\r\n\r\n"
                );
    cmd = cmd.
        arg(reqId).
        arg(order->getOrderId()).
        arg(sign.data()).
        arg(signDt);

    //
    // Send
    //
    bool res = writeToSock( cmd.toLatin1() );
    if ( ! res ) {
        qWarning("send failed!");
        // Lock
        QWriteLocker wLocker( &m_rwLock );
        Q_ASSERT(m_ordersOperations->contains(reqId));
        OrderOpWithPhase orderPhasePtr = m_ordersOperations->take(reqId);
        // Back to accepted
        Order::State oldState = order->getOrderState();
        order->setOrderState( Order::AcceptedState );
        op.m_op->wakeUpAll( Order::ErrorResult );
        // Unlock
        wLocker.unlock();
        emit onOrderStateChanged( ADConnection::Order(order),
                                  oldState,
                                  Order::AcceptedState );
        emit onOrderOperationResult( ADConnection::Order(order),
                                     op,
                                     Order::ErrorResult );

        QThread::quit();
        return;
    }
}

ADConnection::Order::Operation ADConnection::tradePaper ( ADConnection::Order& orderOut,
                                                          const QString& accCode,
                                                          ADConnection::Order::Type tradeType,
                                                          const QString& code,
                                                          quint32 qty, float price )
{
    // Lock
    QWriteLocker wLocker( &m_rwLock );

    // Iterate request
    RequestId reqId = ++m_reqId;

    // Create order
    ADSmartPtr<ADOrderPrivate> orderPtr( new ADOrderPrivate(accCode, tradeType, code, qty, price) );

    // Create order operation
    ADSmartPtr<ADOrderOperationPrivate> op(
                                         new ADOrderOperationPrivate(reqId, ADConnection::Order::CreateOrder, orderPtr) );

    // Save drop request
    m_ordersOperations->insert(reqId, OrderOpWithPhase(op, 0));

    // Change status to accepting
    Order::State oldState = orderPtr->getOrderState();
    orderPtr->setOrderState( Order::AcceptingState );

    // Unlock
    wLocker.unlock();

    emit onOrderStateChanged( ADConnection::Order(orderPtr),
                              oldState,
                              Order::AcceptingState );

    // Save order
    orderOut = ADConnection::Order( orderPtr );

    if ( QThread::currentThread() == this ) {
        tradePaper(op);
    }
    else {
        emit onTradePaper(op);
    }

    return ADConnection::Order::Operation(op);
}

void ADConnection::tradePaper ( ADConnection::Order::Operation op )
{
    Q_ASSERT(op.isValid());
    ADSmartPtr<ADOrderPrivate> order = op.m_op->getOrder();
    Q_ASSERT(order.isValid());
    ADConnection::RequestId reqId = op.m_op->getRequestId();

    //XXX
    QString accCode = "13272-000";
    QString placeCode = "FORTS";

    ADTemplateParser dp;
    QSqlQuery query;
    QMap<QString, QVariant> where;
    QSqlRecord row;
    double multdiv = 0.0;
    QDateTime dt;
    QString tmplDoc;
    QByteArray sign;
    QString cmd;
    bool res = false;
    unsigned int size = 0;
    char* data = 0;

    // Get new_order template
    where.insert("doc_id", "new_order");
    if ( !_sqlExecSelect("AD_DOC_TEMPLATES", where, query) || !query.next() ) {
        goto err;
    }
    where.clear();

    row = query.record();
    tmplDoc = row.value("data").toString().replace("\r", "\r\n");
    if ( tmplDoc.isEmpty() ) {
        qWarning("Error: template document is empty!");
        goto err;
    }

    dp.addParam("blank", "L");
    dp.addParam("b_s", (order->getOrderType() == Order::Buy ? "B" : "S"));
    dp.addParam("acc_code", accCode);
    dp.addParam("place_code", placeCode);
    dp.addParam("p_code", order->getOrderPaperCode());
    dp.addParam("price_currency", "RUR");
    dp.addParam("limits_check", "Y");
    dp.addParam("drop_cond", "Y");
    dp.addParam("ch_drop_time", order->getOrderDropDateTime().toString("dd/MM/yyyy hh:mm"));
    dp.addParam("price", QString("%1").arg(order->getOrderPrice()));
    dp.addParam("paper_qty", QString("%1").arg(order->getOrderQty()));

    // Get papers
    where.insert("p_code", order->getOrderPaperCode());
    where.insert("place_code", placeCode);
    if ( !_sqlExecSelect("AD_PAPERS", where, query) || !query.next() ) {
        goto err;
    }
    where.clear();

    row = query.record();
    if ( !row.contains("ts_p_code") ||
         row.isNull("ts_p_code") ||
         !row.value("ts_p_code").isValid() ||

         !row.contains("mat_date") ||
         row.isNull("mat_date") ||
         !row.value("mat_date").isValid() ) {
        goto err;
    }

    dp.addParam("ts_p_code", row.value("ts_p_code").toString());
    dp.addParam("mat_date", row.value("mat_date").toDateTime().toString("dd/MM/yyyy"));

    // Get subbaccount
    where.insert("acc_code", accCode);
    if ( !_sqlExecSelect("AD_SUB_ACCOUNTS", where, query) || !query.next() ) {
        goto err;
    }
    where.clear();

    row = query.record();
    if ( !row.contains("treaty") ||
         row.isNull("treaty") ||
         !row.value("treaty").isValid() ) {
        goto err;
    }

    dp.addParam("treaty", row.value("treaty").toString());

    // Get account
    where.insert("treaty", row.value("treaty").toInt());
    if ( !_sqlExecSelect("AD_ACCOUNTS", where, query) || !query.next() ) {
        goto err;
    }
    where.clear();

    row = query.record();
    if ( !row.contains("full_name") ||
         row.isNull("full_name") ||
         !row.value("full_name").isValid() ) {
        goto err;
    }

    dp.addParam("client_name", row.value("full_name").toString());


    // Get trade_places
    where.insert("place_code", placeCode);
    if ( !_sqlExecSelect("AD_TRADE_PLACES", where, query) || !query.next() ) {
        goto err;
    }
    where.clear();

    row = query.record();
    if ( !row.contains("market_name") ||
         row.isNull("market_name") ||
         !row.value("market_name").isValid() ||

         !row.contains("place_name") ||
         row.isNull("place_name") ||
         !row.value("place_name").isValid() ) {
        goto err;
    }

    dp.addParam("market_name", row.value("market_name").toString().trimmed());
    dp.addParam("place_name", row.value("place_name").toString().trimmed());
    dp.addParam("dp_name", row.value("dp_name").toString().trimmed());

    // Get position
    where.insert("acc_code", accCode);
    where.insert("place_code", placeCode);
    if ( _sqlExecSelect("AD_POSITIONS", where, query) && query.next() ) {
        row = query.record();
        if ( row.contains("depo_account") &&
             !row.isNull("depo_account") &&
             row.value("depo_account").isValid() )
            dp.addParam("depo_account", row.value("depo_acc").toString());
    }
    where.clear();


    // Get actives
    where.insert("p_code", order->getOrderPaperCode());
    if ( !_sqlExecSelect("AD_ACTIVES", where, query) || !query.next() ) {
        goto err;
    }
    where.clear();

    row = query.record();
    if (
         !row.contains("at_code") ||
         row.isNull("at_code") ||
         !row.value("at_code").isValid() ||

         !row.contains("at_name") ||
         row.isNull("at_name") ||
         !row.value("at_name").isValid() ||

         !row.contains("em_code") ||
         row.isNull("em_code") ||
         !row.value("em_code").isValid() ) {
        goto err;
    }

    dp.addParam("reg_code", row.value("reg_code").toString());
    dp.addParam("at_code", row.value("at_code").toString());
    dp.addParam("at_name", row.value("at_name").toString());
    dp.addParam("contr_descr", row.value("contr_descr").toString());
    if ( !row.contains("divid") ||
         row.isNull("divid") ||
         !row.value("divid").isValid() ||
         row.value("divid").toDouble() == 0.0 ||

         !row.contains("mult") ||
         row.isNull("mult") ||
         !row.value("mult").isValid() ||
         row.value("mult").toDouble() == 0.0 )
        multdiv = 0.0;
    else
        multdiv = row.value("mult").toDouble() / row.value("divid").toDouble();

    dp.addParam("mult_div", QString("%1").arg(multdiv));

    if ( row.contains("strike") &&
         !row.isNull("strike") &&
         row.value("strike").isValid() )
        dp.addParam("strike", row.value("strike").toString());

    // Get emitents
    where.insert("em_code", row.value("em_code").toString());
    if ( !_sqlExecSelect("AD_EMITENTS", where, query) || !query.next() ) {
        qWarning("Error: can't find valid row in AD_EMITENTS table!");
        goto err;
    }
    where.clear();

    row = query.record();
    if ( !row.contains("full_name") ||
         row.isNull("full_name") ||
         !row.value("full_name").isValid() ) {
        qWarning("Error: can't find full_name field!");
        goto err;
    }

    dp.addParam("em_name", row.value("full_name").toString().trimmed());
    dp.addParam("manager_name", m_sessInfo.fullName);
    dp.addParam("sys_name", m_login);

    dt = QDateTime::currentDateTime();
    dp.addParam("sign_time", dt.toString("dd/MM/yyyy hh:mm"));

    // Parse and convert to Windows-1251
    sign = m_win1251Codec->fromUnicode( dp.parse(tmplDoc) );

    //
    // Make signature
    //
    res = m_adLib->makeSignature(m_sessInfo.provCtx, m_sessInfo.certCtx,
                                 sign.data(), sign.size(),
                                 &data, &size);
    if ( !res || data == 0 || size == 0 ) {
        qWarning("Error while sign!");
        goto err;
    }

    sign = QByteArray( data, size );
    m_adLib->freeMemory( data );

    //
    // Create url
    //
    cmd =
        "id=" + QString("%1").arg(reqId) + "|NewOrder\r\n" +
        "blank=" + dp.params()["blank"] +
        "&b_s=" + dp.params()["b_s"] +
        "&acc_code=" + dp.params()["acc_code"] +
        "&place_code=" + dp.params()["place_code"] +
        "&p_code=" + dp.params()["p_code"] +
        "&price_currency=" + dp.params()["price_currency"] +
        "&ch_drop_time=" + dp.params()["ch_drop_time"] +
        "&price=" + dp.params()["price"] +
        "&paper_qty=" + dp.params()["paper_qty"] +
        "&sign=" + QString(sign) +
        "&sign_time=" + dp.params()["sign_time"] +
        "&is_new_template=Y";

    res = writeToSock( cmd.toLatin1() );
    if ( ! res ) {
        goto err;
    }

    return;

err:
    // Lock
    QWriteLocker wLocker( &m_rwLock );

    Q_ASSERT(m_ordersOperations->contains(reqId));
    OrderOpWithPhase orderPhasePtr = m_ordersOperations->take(reqId);
    // To cancel
    Order::State oldState = order->getOrderState();
    order->setOrderState( Order::CancelledState );
    op.m_op->wakeUpAll( Order::ErrorResult );
    // Unlock
    wLocker.unlock();

    emit onOrderStateChanged( ADConnection::Order(order),
                              oldState,
                              Order::CancelledState );
    emit onOrderOperationResult( ADConnection::Order(order),
                                 op,
                                 Order::ErrorResult );
}


bool ADConnection::logQuote ( bool isFutUpdate, const QDateTime& nowDt,
                              const ADFutures& fut, const ADConnection::Quote& futQuote,
                              const ADOption& opt, const ADConnection::Quote& optQuote,
                              float impl_vol, float sell_impl_vol, float buy_impl_vol )
{
    if ( QThread::currentThread() == this ) {
        return sqlLogQuote( isFutUpdate, nowDt, fut, futQuote, opt, optQuote,
                            impl_vol, sell_impl_vol, buy_impl_vol );
    }
    else {
        ADSmartPtr<LogParam> logParam( new LogParam(isFutUpdate, nowDt, fut, futQuote,
                                                  opt, optQuote, impl_vol, sell_impl_vol,
                                                  buy_impl_vol) );
        emit onLogQuote( logParam );
        return true;
    }
}

bool ADConnection::sqlLogQuote ( bool isFutUpdate, const QDateTime& nowDt,
                                 const ADFutures& fut, const ADConnection::Quote& futQuote,
                                 const ADOption& opt, const ADConnection::Quote& optQuote,
                                 float impl_vol, float sell_impl_vol, float buy_impl_vol )
{
    QString sql("INSERT INTO ad_log_quote VALUES ( "
                "GEN_ID(GEN_AD_LOG_QUOTE_ID, 1), :quote_update_type, :log_dt, "
                ":fut_no, :fut_code, :fut_last_price, :fut_best_sell_price, :fut_best_buy_price, "
                ":fut_sellers, :fut_buyers, :opt_no, :opt_code, :opt_type, :opt_strike, :opt_mat_dt, "
                ":opt_last_price, :opt_best_sell_price, :opt_best_buy_price, :opt_sellers, :opt_buyers, "
                ":opt_impl_vol, :opt_sell_impl_vol, :opt_buy_impl_vol )");

    // Execute query
    QSqlQuery query( m_adDB );
    query.prepare( sql );
    query.bindValue(":quote_update_type", (isFutUpdate ? "F" : "O"));
    query.bindValue(":log_dt", nowDt);
    query.bindValue(":fut_no", fut.paperNo);
    query.bindValue(":fut_code", fut.paperCode);
    query.bindValue(":fut_last_price", futQuote.lastPrice);
    query.bindValue(":fut_best_sell_price", futQuote.getBestSeller());
    query.bindValue(":fut_best_buy_price", futQuote.getBestBuyer());
    query.bindValue(":fut_sellers", futQuote.toStringBestSellers(4));
    query.bindValue(":fut_buyers", futQuote.toStringBestBuyers(4));
    query.bindValue(":opt_no", opt.paperNo);
    query.bindValue(":opt_code", opt.paperCode);
    query.bindValue(":opt_type", (opt.type == ADOption::Call ? "C" : "P"));
    query.bindValue(":opt_strike", opt.strike);
    query.bindValue(":opt_mat_dt", opt.matDate);
    query.bindValue(":opt_last_price", optQuote.lastPrice);
    query.bindValue(":opt_best_sell_price", optQuote.getBestSeller());
    query.bindValue(":opt_best_buy_price", optQuote.getBestBuyer());
    query.bindValue(":opt_sellers", optQuote.toStringBestSellers(4));
    query.bindValue(":opt_buyers", optQuote.toStringBestBuyers(4));
    query.bindValue(":opt_impl_vol", impl_vol);
    query.bindValue(":opt_sell_impl_vol", sell_impl_vol);
    query.bindValue(":opt_buy_impl_vol", buy_impl_vol);

    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

bool ADConnection::sqlInsertArchivePaper ( int paperNo, const QString& paperCode,
                                           const QString& tsPaperCode, const QString& placeCode,
                                           const QString& placeName, int unused,
                                           const QString& expired, const QString& boardCode,
                                           const QString& atCode, const QDateTime& matDate )
{
    QString sql("INTO AD_ARCHIVE_PAPERS VALUES ( "
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ? )");

    if ( m_adDB.driverName() == "QIBASE" )
        sql = "UPDATE OR INSERT " + sql;
    else if ( m_adDB.driverName() == "QSQLITE" )
        sql = "INSERT OR REPLACE " + sql;
    else if ( m_adDB.driverName() == "QMYSQL" )
        sql = "INSERT " + sql +
            QString(" ON DUPLICATE KEY UPDATE "
                    " \"paper_no\" = ?, \"p_code\" = ?, "
                    " \"ts_p_code\" = ?, \"place_code\" = ?, "
                    " \"place_name\" = ?, \"unused\" = ?, "
                    " \"expired\" = ?, \"board_code\" = ?, "
                    " \"at_code\" = ?, \"mat_date\" = ? ");
    else {
        qFatal("Unsupported DB driver '%s'",
               qPrintable(m_adDB.driverName()));
        Q_ASSERT(0);
    }

    // Execute query
    QSqlQuery query( m_adDB );
    query.prepare( sql );
    query.addBindValue( paperNo );
    query.addBindValue( paperCode );
    query.addBindValue( tsPaperCode );
    query.addBindValue( placeCode );
    query.addBindValue( placeName );
    query.addBindValue( unused );
    query.addBindValue( expired );
    query.addBindValue( boardCode );
    query.addBindValue( atCode );
    query.addBindValue( matDate );

    // Update bindings for MySQL
    if ( m_adDB.driverName() == "QMYSQL" ) {
        query.addBindValue( paperNo );
        query.addBindValue( paperCode );
        query.addBindValue( tsPaperCode );
        query.addBindValue( placeCode );
        query.addBindValue( placeName );
        query.addBindValue( unused );
        query.addBindValue( expired );
        query.addBindValue( boardCode );
        query.addBindValue( atCode );
        query.addBindValue( matDate );
    }

    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s, SQL: %s",
                 qPrintable(query.lastError().text()),
                 qPrintable(query.executedQuery()));
        return false;
    }
    return true;
}

bool ADConnection::getCachedTemplateDocument ( const QString& docName,
                                               QByteArray& doc )
{
    // Lock
    QReadLocker rdLocker( &m_rwLock );
    bool docExists = m_docTemplates.contains(docName);
    if ( docExists ) {
        doc = m_docTemplates[docName];
        return true;
    }

    // Unlock
    rdLocker.unlock();

    //
    // Try to cache
    //

    //XXX
    QString sql = "SELECT \"data\" FROM AD_DOC_TEMPLATES AS d"
        " WHERE d.\"doc_id\" = :doc_name";
    // Execute query
    QSqlQuery query( m_adDB );
    query.prepare( sql );
    query.bindValue(":doc_name", docName);
    if ( ! query.exec() ) {
        qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
        return false;
    }
    if ( query.next() ) {
        doc = query.value(0).toByteArray();
        // Lock
        QWriteLocker wrLocker( &m_rwLock );
        if ( m_docTemplates.contains(docName) )
            qWarning("Document template '%s' already stored!",
                     qPrintable(docName));
        else
            m_docTemplates[docName] = doc;

        return true;
    }
    else {
        qWarning("Can't find template document '%s'!",
                 qPrintable(docName));
        return false;
    }
}

bool ADConnection::updateSessionInfo ( const QString& infoStr )
{
    bool res = false;

    QStringList fields = infoStr.split("|");
    if ( fields.size() < 15 ) {
        qWarning("Wrong buffer!");
        return false;
    }
    ADSessionInfo info;

    bool ok = false;

    info.message = fields[0];
    info.id = fields[1].toInt(&ok);
    if ( ! ok ) {
        qWarning("can't parse id!");
        return false;
    }
    // Session key in latin1 (actually it is in hex string representation)
    info.sessionKey = QByteArray::fromHex( fields[2].toLatin1() );
    int adTime = fields[3].toInt(&ok);
    if ( ! ok || ! ADTimeToDateTime(adTime, info.serverTime) ) {
        qWarning("can't parse server time!");
        return false;
    }
    // Cert data in latin1 (actually it is in hex string representation)
    info.certData = fields[4].toLatin1();
    info.nickName = fields[6];
    info.cspSN = fields[7];
    info.cspKey = fields[8];
    QStringList ips = fields[9].split(";", QString::SkipEmptyParts);
    if ( ips.size() == 0 ) {
        qWarning("server url is invalid!");
        return false;
    }
    QUrl serverUrl( "https://" + ips[0] );
    if ( ! serverUrl.isValid() ) {
        qWarning("server url is invalid!");
        return false;
    }
    info.serverHost = serverUrl.host();
    info.serverPort = serverUrl.port();
    info.fullName = fields[10];
    info.accounts = fields[11];
    info.RURtoUSD = fields[12].toDouble(&ok);
    if ( ! ok ) {
        qWarning("can't parse RURtoUSD!");
        return false;
    }
    info.RURtoEUR = fields[13].toDouble(&ok);
    if ( ! ok ) {
        qWarning("can't parse RURtoEUR!");
        return false;
    }
    info.userID = fields[14].toInt(&ok);
    if ( ! ok ) {
        qWarning("can't parse userId!");
        return false;
    }

    // Load certificate
    res = m_adLib->loadCertificate( info.certData.data(), info.certData.size(),
                                    &info.certCtx );
    if ( ! res || info.certCtx == 0 ) {
        qWarning("can't load certificate!");
        m_lastError = CertificateError;
        return false;
    }
    // Load context
    res = m_adLib->loadContext( info.certCtx, &info.provCtx );
    if ( ! res || info.provCtx == 0 ) {
        qWarning("can't load context!");
        m_adLib->unloadCertificate( info.certCtx );
        m_lastError = ContextError;
        return false;
    }

    // Set session info
    m_sessInfo = info;

    // Set server time
    {
        QWriteLocker wLocker( &m_rwLock );
        m_srvTime = info.serverTime;
        m_srvTimeUpdate = QDateTime::currentDateTime();
    }

    return true;
}

void ADConnection::run ()
{
    bool res = false;

    QString login = m_login;
    QString passwd = m_password;

    // Drop states and flags
    m_lastError = NoError;
    m_state = DisconnectedState;
    m_authed = false;
    m_sockBuffer.clear();
    m_sockEncBuffer.clear();
    m_sessInfo = ADSessionInfo();
    m_adDB = QSqlDatabase();
    m_win1251Codec = QTextCodec::codecForName("Windows-1251");

    // Check encoding support
    if ( !m_win1251Codec ) {
        qWarning("Error: can't find Windows-1251 encoding");
        m_lastError = Windows1251DoesNotExistError;
        goto clean;
    }

    // Do bootstraping
    {
        res = ADBootstrap::bootstrap();
        if ( ! res ) {
            qWarning("bootstraping failed!");
            m_lastError = BootstrapError;
            goto clean;
        }
    }

    // Load library
    {
        res = m_adLib->load();
        if ( ! res ) {
            qWarning("can't load dll!");
            m_lastError = DynamicLibLoadError;
            goto clean;
        }
    }

    // Connect to DB
    {
        // Default driver name
        const QString DriverName = "QSQLITE";
        QString dbResource;
        QString dbPath;
        bool dbShouldBeCreated = false;

        if ( DriverName == "QMYSQL" ) {
            m_adDB = QSqlDatabase::addDatabase( DriverName );
            m_adDB.setUserName( "root" );
            m_adDB.setPassword( "" );
            m_adDB.setDatabaseName( "ad");

            dbResource = ":AD.mysql.sql";
        }
        else {
            // Create DB directory
            QDir dbDir( QCoreApplication::applicationDirPath() + "/db" );
            if ( ! dbDir.exists() ) {
                res = dbDir.mkpath(dbDir.absolutePath());
                if ( ! res ) {
                    qWarning("Can't create DB dir '%s'",
                             qPrintable(dbDir.absolutePath()));
                    m_lastError = SQLConnectError;
                    goto clean;
                }
            }

            if ( DriverName == "QSQLITE" ) {
                dbPath = dbDir.absolutePath() + "/AD.db";
                dbResource = ":AD.sqlite.sql";
            }
            else if ( DriverName == "QIBASE" ) {
                dbPath = dbDir.absolutePath() + "/AD.fdb";
                dbResource = ":AD.firebird.sql";
            }
            else
                Q_ASSERT(0);

            dbShouldBeCreated = ! QFileInfo(dbPath).exists();

            // Setup DB
            m_adDB = QSqlDatabase::addDatabase( DriverName );
            m_adDB.setDatabaseName( dbPath );

            if ( DriverName == "QIBASE" )
                m_adDB.setConnectOptions("ISC_DPB_LC_CTYPE=UTF-8");
        }

        // Try to fill DB
        if ( dbShouldBeCreated ) {
            //XXX For now we do not support DB population
            //XXX with batch SQL statements, so return error
            {
                qWarning("DB '%s' does not exist!", qPrintable(dbPath));
                m_lastError = SQLConnectError;
                goto clean;
            }

            QFile sqlFile( dbResource );
            if ( ! sqlFile.open(QFile::ReadOnly) ) {
                qWarning("Can't open SQL resource '%s' for DB creation!",
                         qPrintable(dbResource));
                m_lastError = SQLConnectError;
                goto clean;
            }
            QByteArray sqlData = sqlFile.readAll();
            if ( sqlData.isEmpty() ) {
                qWarning("Can't read SQL resource '%s' for DB creation!",
                         qPrintable(dbResource));
                m_lastError = SQLConnectError;
                goto clean;
            }

            //XXX DB open should be made before any SQL execution
        }

        if ( ! m_adDB.open() ) {
            qWarning("Can't open DB!");
            m_lastError = SQLConnectError;
            goto clean;
        }

        if ( m_adDB.driverName() == "QMYSQL" ) {
            QSqlQuery ansiModeQuery( m_adDB );
            if ( ! ansiModeQuery.exec("SET sql_mode='ANSI_QUOTES'") ) {
                qWarning("SQL ERROR: %s", qPrintable(ansiModeQuery.lastError().text()));
                goto clean;
            }
        }
    }

    // Get session info through HTTPS
    {
        char protoVer[ 256 ] = {0};
        int size = sizeof(protoVer);
        res = m_adLib->getProtocolVersion( protoVer, &size );
        if ( ! res ) {
            qWarning("call getProtoVersion returned false!");
            m_lastError = DynamicLibCallError;
            goto clean;
        }

        QString url("https://www.alfadirect.ru/ads/connect.idc?"
                    // Url from API doc.
                    //"vers=%1&cpcsp_eval=0&cpcsp_ver=3.6").arg(protoVer) );
                    //"vers=3.1.1.8&cpcsp_eval=0&cpcsp_ver=2.0.1.2089") );

                    //Url from AD terminal.
                    "vers=3.5.1.5&cpcsp_eval=0&cpcsp_ver=3.6");
        // Send https request
        QString response;
        Error err = ADSendHttpsRequest(url, m_login, m_password, response);
        if ( err != NoError ) {
            m_lastError = err;
            goto clean;
        }

        // Parse data
        res = updateSessionInfo( response );
        if ( ! res ) {
            m_lastError = ParseHTTPDataError;
            qWarning("HTTP response is invalid!");
            goto clean;
        }
    }

    // Connect to AD server
    {
        SQLReceiver sqlReceiver( this );
        QObject::connect( this,
                          SIGNAL(onFindFutures(const QString&, ADFutures*, bool*)),
                          &sqlReceiver,
                          SLOT(sqlFindFutures(const QString&, ADFutures*, bool*)),
                          Qt::BlockingQueuedConnection );
        QObject::connect( this,
                          SIGNAL(onFindPaperNo(const QString&, bool, int*, bool*)),
                          &sqlReceiver,
                          SLOT(sqlFindPaperNo(const QString&, bool, int*, bool*)),
                          Qt::BlockingQueuedConnection );
        QObject::connect( this,
                          SIGNAL(onLogQuote(ADSmartPtr<ADConnection::LogParam>)),
                          &sqlReceiver,
                          SLOT(sqlLogQuote(ADSmartPtr<ADConnection::LogParam>)),
                          Qt::QueuedConnection );

        GenericReceiver genericReceiver( this );
        QObject::connect( this,
                          SIGNAL(onTradePaper(ADConnection::Order::Operation)),
                          &genericReceiver,
                          SLOT(onTradePaper(ADConnection::Order::Operation)),
                          Qt::BlockingQueuedConnection );
        QObject::connect( this,
                          SIGNAL(onCancelOrder(ADConnection::Order::Operation)),
                          &genericReceiver,
                          SLOT(onCancelOrder(ADConnection::Order::Operation)),
                          Qt::BlockingQueuedConnection );
        QObject::connect( this,
                          SIGNAL(onChangeOrder(ADConnection::Order::Operation,
                                               quint32, float)),
                          &genericReceiver,
                          SLOT(onChangeOrder(ADConnection::Order::Operation,
                                             quint32, float)),
                          Qt::BlockingQueuedConnection );


        QTimer pingTimer;
        m_sock = new QTcpSocket;
        TcpReceiver tcpReceiver( this, *m_sock );

        Q_ASSERT(QThread::currentThread() == this );

        QObject::connect(&pingTimer,
                         SIGNAL(timeout()),
                         &tcpReceiver,
                         SLOT(pingTimer()));

        QObject::connect(m_sock,
                         SIGNAL(readyRead()),
                         &tcpReceiver,
                         SLOT(tcpReadyRead()));
        QObject::connect(m_sock,
                         SIGNAL(error(QAbstractSocket::SocketError)),
                         &tcpReceiver,
                         SLOT(tcpError(QAbstractSocket::SocketError)));
        QObject::connect(m_sock,
                         SIGNAL(stateChanged(QAbstractSocket::SocketState)),
                         &tcpReceiver,
                         SLOT(tcpStateChanged(QAbstractSocket::SocketState)));
        QObject::connect( this,
                          SIGNAL(onWriteToSock(QByteArray)),
                          &tcpReceiver,
                          SLOT(tcpWriteToSock(QByteArray)),
                          Qt::QueuedConnection );

        // Start ping timer
        pingTimer.start( 5 * 1000 );

        // Connect
        m_sock->connectToHost( m_sessInfo.serverHost, m_sessInfo.serverPort );

        // Run into loop
        QThread::exec();

        delete m_sock;
        m_sock = 0;
    }

 clean:
    // Wake up subscribers
    {
        // Lock
        QReadLocker rLocker( &m_rwLock );

        QList<ADSmartPtr<ADSubscriptionPrivate> >::ConstIterator itSub = m_subscriptions->begin();
        for ( ; itSub != m_subscriptions->end(); ++itSub ) {
            //XXX NOT IMPLEMENTED itSub->wakeupAll();
        }

        QHash<RequestId, OrderOpWithPhase>::ConstIterator itOrdOp;
        itOrdOp = m_ordersOperations->begin();
        for ( ; itOrdOp != m_ordersOperations->end(); ++itOrdOp ) {
            //XXX NOT IMPLEMENTED itOrdOp->wakeupAll();
        }

        QHash<Order::OrderId, ADSmartPtr<ADOrderPrivate> >::ConstIterator itOrd =
            m_activeOrders->begin();
        for ( ; itOrd != m_activeOrders->end(); ++itOrd ) {
            //XXX NOT IMPLEMENTED itOrd->wakeupAll();
        }

        //XXX NOT IMPLEMENTED CLEAN ALL LISTS AND HASHES
    }

    // Close DB if it was opened
    if ( m_adDB.isOpen() )
        m_adDB.close();

    // Remove DB from valid connections if connection was created
    if ( m_adDB.isValid() ) {
        QString connName = m_adDB.connectionName();
        m_adDB = QSqlDatabase();
        QSqlDatabase::removeDatabase( connName );
    }

    // Unload AD lib
    if ( m_adLib->isLoaded() ) {
        if ( m_sessInfo.provCtx ) {
            m_adLib->unloadContext( m_sessInfo.provCtx );
            m_sessInfo.provCtx = 0;
        }
        if ( m_sessInfo.certCtx ) {
            m_adLib->unloadCertificate( m_sessInfo.certCtx );
            m_sessInfo.certCtx = 0;
        }
        m_adLib->unload();
    }

    m_state = DisconnectedState;
    emit onStateChanged( DisconnectedState );

    return;
}

//
// Tcp callbacks
//

void ADConnection::tcpReadyRead ( QTcpSocket& )
{
    QList<DataBlock> recv;
    bool fullResp = parseReceivedData( recv );
    if ( ! fullResp )
        // Wait next chunk
        return;

    if ( recv.size() == 0 ) {
        qWarning("empty block list!");
        Q_ASSERT(0);
        return;
    }

    //XXXX
    QString XXX_portfolio = "XXXX-0000";
    QString XXX_paper_code = "XXXX-0000";

    // Check if not authed
    if ( ! m_authed ) {
        bool res = parseAuthResponse( recv[0] );
        if ( ! res ) {
            m_lastError = AuthError;
            qWarning("Auth error!");
            QThread::quit();
            return;
        }

        //Set default AD options
        setMinADDelay();
        QHash<QString, int> simpleFilter;
        getADLastSimpleFilterUpdates( simpleFilter );
        setADFilters( simpleFilter );

        m_authed = true;
        m_state = ConnectedState;
        emit onStateChanged( ConnectedState );

        // Find active orders and insert them
        QList< ADSmartPtr<ADOrderPrivate> > activeOrders;
        if ( _sqlFindActiveOrders(XXX_portfolio, XXX_paper_code, activeOrders) ) {
            foreach ( ADSmartPtr<ADOrderPrivate> order, activeOrders ) {
                // Lock
                QWriteLocker wLocker( &m_rwLock );
                Order::OrderId orderId = order->getOrderId();
                Q_ASSERT(!m_activeOrders->contains(orderId));
                m_activeOrders->insert(orderId, order);
                order->setOrderState( Order::AcceptedState );
                // Unlock
                wLocker.unlock();
                emit onOrderStateChanged( ADConnection::Order(order),
                                          Order::UnknownState,
                                          Order::AcceptedState );
            }
        }

        // Find current position
        Position position;
        if ( _sqlGetCurrentPosition(XXX_portfolio, XXX_paper_code, position) ) {
            // Lock
            QWriteLocker wLocker( &m_rwLock );
            m_positions[position.accCode][position.paperCode] = position;
            // Unlock
            wLocker.unlock();
            emit onPositionChanged( position.accCode, position.paperCode, position.paperNo );
        }

        return;
    }

    // Firstly store all received data into DB
    storeDataIntoDB( recv );

    QList<DataBlock>::Iterator it = recv.begin();
    for ( ; it != recv.end(); ++it ) {
        /// Parse server time
        if ( it->blockName.contains(ADBlockName::SRV_TIME) ) {
            bool ok = false;
            QDateTime dt;
            int adTime = it->blockData.toInt(&ok);
            if ( ! ok || ! ADTimeToDateTime(adTime, dt) ) {
                qWarning("Wrong block: %s", ADBlockName::SRV_TIME);
                continue;
            }
            // Lock
            QWriteLocker wLocker( &m_rwLock );
            m_srvTime = dt;
            m_srvTimeUpdate = QDateTime::currentDateTime();
        }
        /// Parse orders queue: {paper_no, price, buy_qty, sell_qty, i_last_update, yield}
        else if ( it->blockName.contains(ADBlockName::ORDERS_QU_INIT) ||
                  it->blockName.contains(ADBlockName::ORDERS_QU_0) ||
                  it->blockName.contains(ADBlockName::ORDERS_QU_1) ) {

            bool initQueue = it->blockName.contains(ADBlockName::ORDERS_QU_INIT);
            QSet<int> updates;

            QStringList lines = it->blockData.split("\n", QString::SkipEmptyParts);
            for ( QStringList::Iterator it = lines.begin();
                  it != lines.end(); ++it ) {
                QString& line = *it;
                QStringList cols = line.split("|");

                bool ok = (cols.size() >= 4);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse queue line!");
                    continue;
                }

                int paperNo = cols[0].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse queue paper no!");
                    continue;
                }
                float price = cols[1].toFloat(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse queue price!");
                    continue;
                }
                int buyQty = cols[2].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse queue buy qty!");
                    continue;
                }
                int sellQty = cols[3].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse queue sell qty!");
                    continue;
                }

                // Lock
                QWriteLocker wLocker( &m_rwLock );
                Quote& quote = m_quotes[paperNo];
                if ( initQueue && ! updates.contains(paperNo) ) {
                    quote.buyers.clear();
                    quote.sellers.clear();
                    quote.bidOffers.clear();
                }
                quote.paperNo = paperNo;

                // Mark as updated
                updates.insert(paperNo);

                // Fill buyers
                if ( buyQty > 0 )
                    quote.buyers[price] = buyQty;
                else
                    quote.buyers.remove(price);

                // Fill sellers
                if ( sellQty > 0 )
                    quote.sellers[price] = sellQty;
                else
                    quote.sellers.remove(price);

                // Fill bid/offer
                if ( buyQty > 0 || sellQty > 0 )
                    quote.bidOffers[price] = BidOffer(price, buyQty, sellQty);
                else
                    quote.bidOffers.remove(price);

                // Update subscriptions
                QList<ADSmartPtr<ADSubscriptionPrivate> >::Iterator itSub =
                    m_subscriptions->begin();
                while ( itSub != m_subscriptions->end() ) {
                    if ( (*itSub).countRefs() > 1 ) {
                        (*itSub)->update(paperNo, Subscription::QueueSubscription);
                        ++itSub;
                    }
                    else {
                        // Unsubscribe and drop subscription
                        _unsubscribeToQuote( *itSub );
                        itSub = m_subscriptions->erase( itSub );
                    }
                }
            }

            foreach ( int paperNo, updates )
                emit onQuoteReceived( paperNo, Subscription::QueueSubscription );
        }
        /// Parse quotes: {paper_no, open_price, last_price, ...}
        else if ( it->blockName.contains(ADBlockName::QUOTE_INIT) ||
                  it->blockName.contains(ADBlockName::QUOTE_0) ||
                  it->blockName.contains(ADBlockName::QUOTE_1) ||
                  it->blockName.contains(ADBlockName::QUOTE_2) ||
                  it->blockName.contains(ADBlockName::QUOTE_3) ) {

            QSet<int> updates;

            QStringList lines = it->blockData.split("\n", QString::SkipEmptyParts);
            for ( QStringList::Iterator it = lines.begin();
                  it != lines.end(); ++it ) {
                QString& line = *it;
                QStringList cols = line.split("|");

                bool ok = (cols.size() >= 3);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse quote line!");
                    continue;
                }

                int paperNo = cols[0].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse quote paper no!");
                    continue;
                }
                bool lastPriceNotZero = false;
                float lastPrice = cols[2].toFloat(&lastPriceNotZero);
                if ( ! lastPriceNotZero ) {
                    continue;
                }

                // Lock
                QWriteLocker wLocker( &m_rwLock );
                Quote& quote = m_quotes[paperNo];
                quote.paperNo = paperNo;
                quote.lastPrice = lastPrice;

                // Update subscriptions
                QList<ADSmartPtr<ADSubscriptionPrivate> >::Iterator itSub =
                    m_subscriptions->begin();
                while ( itSub != m_subscriptions->end() ) {
                    if ( (*itSub).countRefs() > 1 ) {
                        (*itSub)->update(paperNo, Subscription::QuoteSubscription);
                        ++itSub;
                    }
                    else {
                        // Unsubscribe and drop subscription
                        _unsubscribeToQuote( *itSub );
                        itSub = m_subscriptions->erase( itSub );
                    }
                }

                // Unlock
                wLocker.unlock();

                // Mark as updated
                updates.insert(paperNo);
            }

            foreach ( int paperNo, updates )
                emit onQuoteReceived( paperNo, Subscription::QuoteSubscription );
        }
        /// Parse system responses
        else if ( it->blockName.contains(ADBlockName::MSG_ID) ) {
            QStringList cols = it->blockData.split("|");
            if ( cols.size() < 2 ) {
                qWarning("Wrong block '%s': block data is wrong!", qPrintable(it->blockName));
                continue;
            }
            bool ok = false;
            RequestId id = cols[0].toInt(&ok);
            if ( ! ok ) {
                qWarning("Wrong block '%s': can't parse request id!", qPrintable(it->blockName));
                continue;
            }

            // Lock
            QWriteLocker wLocker( &m_rwLock );

            // Check id
            if ( ! m_ordersOperations->contains(id) )
                continue;

            // Get
            OrderOpWithPhase& orderPhasePtr = m_ordersOperations->operator[](id);

            // Check order creation
            if ( orderPhasePtr.first->getOperationType() == Order::CreateOrder ) {
                // Handle broker sucess response for 0 phase
                if ( orderPhasePtr.second == 0 &&
                     cols[1].contains(QRegExp("^OK$")) ) {
                    orderPhasePtr.second = 1;
                    continue;
                }
                // Handle broker sucess response for 1 phase
                else if ( orderPhasePtr.second == 1 &&
                          cols[1].contains(QRegExp(QString::fromUtf8("^Заявка принята к исполнению$"))) ) {
                    orderPhasePtr.second = 2;
                    continue;
                }
                // Handle exchange sucess response for 2 phase
                else if ( orderPhasePtr.second == 2 &&
                          cols[1].contains(QRegExp(QString::fromUtf8("принята Системой.$"))) ) {
                    if ( cols.size() < 5 ) {
                        qWarning("!!!! Response on request id '%d' is wrong!", id);
                        //XXX I don't know what to do in this case!
                        goto create_order_error;
                    }
                    Order::OrderId orderId = cols[4].toInt(&ok);
                    if ( ! ok ) {
                        qWarning("!!!! Response on request id '%d' is wrong!", id);
                        //XXX I really don't know what to do in this case!
                        goto create_order_error;
                    }

                    // Accepted
                    ADSmartPtr<ADOrderOperationPrivate> orderOpPtr = orderPhasePtr.first;
                    ADSmartPtr<ADOrderPrivate> order = orderOpPtr->getOrder();
                    m_ordersOperations->take(id);
                    bool inActive = m_activeOrders->contains(orderId);
                    Order::State oldState = Order::UnknownState;
                    // Check if already handled by statuses.
                    if ( ! inActive ) {
                        m_activeOrders->insert(orderId, order);
                        oldState = order->getOrderState();
                        order->setOrderState( Order::AcceptedState, orderId );
                    }
                    orderOpPtr->wakeUpAll( Order::SuccessResult );
                    // Unlock
                    wLocker.unlock();
                    if ( ! inActive )
                        emit onOrderStateChanged( ADConnection::Order(order),
                                                  oldState,
                                                  Order::AcceptedState );
                    emit onOrderOperationResult( ADConnection::Order(order),
                                                 ADConnection::Order::Operation(orderOpPtr),
                                                 Order::SuccessResult );
                }
                // Handle broker other responses (assume errors)
                else {
                create_order_error:
                    ADSmartPtr<ADOrderOperationPrivate> orderOpPtr = orderPhasePtr.first;
                    ADSmartPtr<ADOrderPrivate> order = orderOpPtr->getOrder();
                    m_ordersOperations->take(id);
                    Order::State oldState = order->getOrderState();
                    order->setOrderState( Order::CancelledState );
                    orderOpPtr->wakeUpAll( Order::ErrorResult );
                    // Unlock
                    wLocker.unlock();
                    emit onOrderStateChanged( ADConnection::Order(order),
                                              oldState,
                                              Order::CancelledState );
                    emit onOrderOperationResult( ADConnection::Order(order),
                                                 ADConnection::Order::Operation(orderOpPtr),
                                                 Order::ErrorResult );
                }

            }
            // Check change orders requests
            else if ( orderPhasePtr.first->getOperationType() == Order::ChangeOrder ) {
                // Handle broker sucess response for 0 phase
                if ( orderPhasePtr.second == 0 &&
                     cols[1].contains(QRegExp("^OK$")) ) {
                    orderPhasePtr.second = 1;
                    continue;
                }
                // Handle broker sucess response for 1 phase
                else if ( orderPhasePtr.second == 1 &&
                          cols[1].contains(QRegExp(QString::fromUtf8("^Заявка принята к изменению$"))) ) {
                    orderPhasePtr.second = 2;
                    continue;
                }
                // Handle exchange sucess response for 2 phase
                else if ( orderPhasePtr.second == 2 &&
                          cols[1].contains(QRegExp(QString::fromUtf8("успешно изменена,"))) ) {
                    if ( cols.size() < 5 ) {
                        qWarning("!!!! Response on request id '%d' is wrong!", id);
                        //XXX I don't know what to do in this case!
                        goto change_order_error;
                    }
                    Order::OrderId newOrderId = cols[4].toInt(&ok);
                    if ( ! ok ) {
                        qWarning("!!!! Response on request id '%d' is wrong!", id);
                        //XXX I really don't know what to do in this case!
                        goto change_order_error;
                    }

                    if ( m_activeOrders->contains(newOrderId) ) {
                        qWarning("!!!! New order id '%d' on request id '%d' is wrong!",
                                 newOrderId, id);
                        //XXX I really don't know what to do in this case!
                        goto change_order_error;
                    }

                    // Change
                    ADSmartPtr<ADOrderOperationPrivate> orderOpPtr = orderPhasePtr.first;
                    ADSmartPtr<ADOrderPrivate> order = orderOpPtr->getOrder();
                    m_ordersOperations->take(id);
                    Order::State oldState = order->getOrderState();
                    quint32 qty = 0;
                    float price = 0.0;
                    order->getPresaveValues(qty, price);
                    // Save new order id, qty and price
                    order->setOrderState( Order::AcceptedState, newOrderId, qty, price );
                    orderOpPtr->wakeUpAll( Order::SuccessResult );
                    // Unlock
                    wLocker.unlock();
                    emit onOrderStateChanged( ADConnection::Order(order),
                                              oldState,
                                              Order::AcceptedState );
                    emit onOrderOperationResult( ADConnection::Order(order),
                                                 ADConnection::Order::Operation(orderOpPtr),
                                                 Order::SuccessResult );
                }
                // Handle broker other responses (assume errors)
                else {
                change_order_error:
                    ADSmartPtr<ADOrderOperationPrivate> orderOpPtr = orderPhasePtr.first;
                    ADSmartPtr<ADOrderPrivate> order = orderOpPtr->getOrder();
                    m_ordersOperations->take(id);
                    Order::State oldState = order->getOrderState();
                    // Set status again to accepted because cancellation failed!
                    order->setOrderState( Order::AcceptedState );
                    orderOpPtr->wakeUpAll( Order::ErrorResult );
                    // Unlock
                    wLocker.unlock();
                    emit onOrderStateChanged( ADConnection::Order(order),
                                              oldState,
                                              Order::AcceptedState );
                    emit onOrderOperationResult( ADConnection::Order(order),
                                                 ADConnection::Order::Operation(orderOpPtr),
                                                 Order::ErrorResult );
                }
            }
            // Check drop orders requests
            else if ( orderPhasePtr.first->getOperationType() == Order::CancelOrder ) {
                // Handle broker sucess response for 0 phase
                if ( orderPhasePtr.second == 0 &&
                     cols[1].contains(QRegExp("^OK$")) ) {
                    orderPhasePtr.second = 1;
                    continue;
                }
                // Handle broker sucess response for 1 phase
                else if ( orderPhasePtr.second == 1 &&
                          cols[1].contains(QRegExp(QString::fromUtf8("^Заявка принята к удалению$"))) ) {
                    orderPhasePtr.second = 2;
                    continue;
                }
                // Handle exchange sucess response for 2 phase
                else if ( orderPhasePtr.second == 2 &&
                          (cols[1].contains(QRegExp(QString::fromUtf8("помечена к удалению.$"))) ||
                           cols[1].contains(QRegExp(QString::fromUtf8("уже удалена.$")))) ) {
                    if ( cols.size() < 5 ) {
                        qWarning("!!!! Response on request id '%d' is wrong!", id);
                        //XXX I don't know what to do in this case!
                        goto drop_order_error;
                    }
                    Order::OrderId orderId = cols[4].toInt(&ok);
                    if ( ! ok ) {
                        qWarning("!!!! Response on request id '%d' is wrong!", id);
                        //XXX I really don't know what to do in this case!
                        goto drop_order_error;
                    }

                    if ( ! m_activeOrders->contains(orderId) ) {
                        qWarning("!!!! Order id '%d' on request id '%d' is wrong!",
                                 orderId, id);
                        //XXX I really don't know what to do in this case!
                        goto drop_order_error;
                    }

                    // Remove
                    ADSmartPtr<ADOrderOperationPrivate> orderOpPtr = orderPhasePtr.first;
                    ADSmartPtr<ADOrderPrivate> order = orderOpPtr->getOrder();
                    m_ordersOperations->take(id);
                    m_activeOrders->take(orderId);
                    m_inactiveOrders->insert(orderId, order);
                    Order::State oldState = order->getOrderState();
                    order->setOrderState( Order::CancelledState );
                    orderOpPtr->wakeUpAll( Order::SuccessResult );
                    // Unlock
                    wLocker.unlock();
                    emit onOrderStateChanged( ADConnection::Order(order),
                                              oldState,
                                              Order::CancelledState );
                    emit onOrderOperationResult( ADConnection::Order(order),
                                                 ADConnection::Order::Operation(orderOpPtr),
                                                 Order::SuccessResult );
                }
                // Handle broker other responses (assume errors)
                else {
                drop_order_error:
                    ADSmartPtr<ADOrderOperationPrivate> orderOpPtr = orderPhasePtr.first;
                    ADSmartPtr<ADOrderPrivate> order = orderOpPtr->getOrder();
                    m_ordersOperations->take(id);
                    Order::State oldState = order->getOrderState();
                    // Set status again to accepted because cancellation failed!
                    order->setOrderState( Order::AcceptedState );
                    orderOpPtr->wakeUpAll( Order::ErrorResult );
                    // Unlock
                    wLocker.unlock();
                    emit onOrderStateChanged( ADConnection::Order(order),
                                              oldState,
                                              Order::AcceptedState );
                    emit onOrderOperationResult( ADConnection::Order(order),
                                                 ADConnection::Order::Operation(orderOpPtr),
                                                 Order::ErrorResult );
                }
            }
        }
        // All trades
        else if ( it->blockName.contains(ADBlockName::ALL_TRADES) ) {
        }
        // My orders
        else if ( it->blockName.contains(ADBlockName::MY_ORDERS) ) {
            QStringList lines = it->blockData.split("\n", QString::SkipEmptyParts);
            for ( QStringList::Iterator it = lines.begin();
                  it != lines.end(); ++it ) {
                QString& line = *it;
                QStringList cols = line.split("|");

                bool ok = (cols.size() >= 29);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse my_orders line!");
                    continue;
                }

                Order::OrderId orderId = cols[0].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse orderId of my_orders line!");
                    continue;
                }

                QString accCode = cols[1];
                QString status = cols[3];
                QString b_s = cols[4];
                float price = cols[5].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse price of my_orders line!");
                    continue;
                }
                int qty = cols[6].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse qty of my_orders line!");
                    continue;
                }

                bool restQtyOk = false;
                int restQty = cols[8].toInt(&restQtyOk);
                if ( restQtyOk && restQty > qty ) {
                    qWarning("Wrong block line: can't parse restQty of my_orders line!");
                    continue;
                }

                QString paperCode = cols[14];
                QDateTime dropDt = QDateTime::fromString( cols[28], "dd/MM/yyyy hh:mm:ss" );
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse dropDt of my_orders line!");
                    continue;
                }
                Order::Type orderBS = (b_s == "S" ? Order::Sell : Order::Buy);

                // Lock
                QWriteLocker wLocker( &m_rwLock );

                //
                // Create order if is not created yet
                //
                if ( (status == "O" || status == "X" || status == "N") &&
                     ! m_inactiveOrders->contains(orderId) ) {
                    ADSmartPtr<ADOrderPrivate> order;
                    bool newlyAccepted = false;
                    quint32 trades = 0;
                    // If order newly created
                    if ( ! m_activeOrders->contains(orderId) ) {
                        newlyAccepted = true;
                        order = ADSmartPtr<ADOrderPrivate>( new ADOrderPrivate(accCode, orderBS, paperCode, qty,
                                                                             price, dropDt, orderId) );
                        m_activeOrders->insert(orderId, order);
                        order->setOrderState( Order::AcceptedState );
                    }
                    // If exists
                    else {
                        order = m_activeOrders->value( orderId );
                    }

                    // Check rest qty
                    if ( restQtyOk ) {
                        trades = order->updateRestQty( restQty );
                    }

                    // Unlock
                    wLocker.unlock();

                    if ( newlyAccepted )
                        emit onOrderStateChanged( ADConnection::Order(order),
                                                  Order::UnknownState,
                                                  Order::AcceptedState );
                    if ( trades > 0 )
                        emit onTrade( ADConnection::Order(order), trades );
                }
                //
                // Check if order is executed
                //
                else if ( status == "M" ) {
                    ADSmartPtr<ADOrderPrivate> orderPtr;
                    QList< ADSmartPtr<ADOrderOperationPrivate> > orderOps;
                    Order::State oldState = Order::UnknownState;
                    quint32 trades = 0;
                    // Mark active order as executed
                    if ( m_activeOrders->contains(orderId) ) {
                        // Remove
                        orderPtr = m_activeOrders->take(orderId);
                        m_inactiveOrders->insert(orderId, orderPtr);

                        // Set status
                        oldState = orderPtr->getOrderState();
                        trades = orderPtr->updateRestQty( 0 );
                        orderPtr->setOrderState( Order::ExecutedState, orderId );
                    }

                    // Try to find pended operations
                    _findOperationsByOrderId(orderId, orderOps);
                    // Wake up all operations id exist
                    QList< ADSmartPtr<ADOrderOperationPrivate> >::Iterator it = orderOps.begin();
                    for ( ; it != orderOps.end(); ++it ) {
                        ADSmartPtr<ADOrderOperationPrivate>& op = *it;
                        op->wakeUpAll( Order::ErrorResult );
                        m_ordersOperations->remove(op->getRequestId());
                    }

                    // Unlock
                    wLocker.unlock();

                    // Emit signal for order
                    if ( orderPtr.isValid() ) {
                        emit onOrderStateChanged( ADConnection::Order(orderPtr),
                                                  oldState,
                                                  Order::ExecutedState );

                        // Iterate over other operations and emit signals
                        QList< ADSmartPtr<ADOrderOperationPrivate> >::Iterator it = orderOps.begin();
                        for ( ; it != orderOps.end(); ++it ) {
                            ADSmartPtr<ADOrderOperationPrivate>& op = *it;
                            emit onOrderOperationResult( ADConnection::Order(orderPtr),
                                                         ADConnection::Order::Operation(op),
                                                         Order::ErrorResult );
                        }

                        // Emit trades
                        if ( trades > 0 ) {
                            emit onTrade( ADConnection::Order(orderPtr), trades );
                        }
                    }
                }
                //
                // Check if order is dropped
                //
                else if ( status == "W" ) {
                    ADSmartPtr<ADOrderPrivate> orderPtr;
                    QList< ADSmartPtr<ADOrderOperationPrivate> > orderOps;
                    // Mark active order as executed
                    if ( m_activeOrders->contains(orderId) ) {
                        // Remove
                        orderPtr = m_activeOrders->take(orderId);
                        m_inactiveOrders->insert(orderId, orderPtr);
                    }

                    Order::State oldState = Order::UnknownState;

                    // Set status
                    if ( orderPtr.isValid() ) {
                        oldState = orderPtr->getOrderState();
                        orderPtr->setOrderState( Order::CancelledState, orderId );
                    }

                    // Try to find pended operations
                    _findOperationsByOrderId(orderId, orderOps);
                    // Wake up all operations id exist
                    QList< ADSmartPtr<ADOrderOperationPrivate> >::Iterator it = orderOps.begin();
                    for ( ; it != orderOps.end(); ++it ) {
                        ADSmartPtr<ADOrderOperationPrivate>& op = *it;
                        if ( op->getOperationType() == Order::CancelOrder )
                            op->wakeUpAll( Order::SuccessResult );
                        else
                            op->wakeUpAll( Order::ErrorResult );
                        m_ordersOperations->remove(op->getRequestId());
                    }

                    // Unlock
                    wLocker.unlock();

                    // Emit signal for order
                    if ( orderPtr.isValid() ) {
                        emit onOrderStateChanged( ADConnection::Order(orderPtr),
                                                  oldState,
                                                  Order::CancelledState );

                        // Iterate over other operations and emit signals
                        QList< ADSmartPtr<ADOrderOperationPrivate> >::Iterator it = orderOps.begin();
                        for ( ; it != orderOps.end(); ++it ) {
                            ADSmartPtr<ADOrderOperationPrivate>& op = *it;
                            if ( op->getOperationType() == Order::CancelOrder )
                                emit onOrderOperationResult( ADConnection::Order(orderPtr),
                                                             ADConnection::Order::Operation(op),
                                                             Order::SuccessResult );
                            else
                                emit onOrderOperationResult( ADConnection::Order(orderPtr),
                                                             ADConnection::Order::Operation(op),
                                                             Order::ErrorResult );
                        }
                    }

                }
                else {
                    // Unknown status!
                }
            }
        }
        // My trades
        else if ( it->blockName.contains(ADBlockName::MY_TRADES) ) {
            QStringList lines = it->blockData.split("\n", QString::SkipEmptyParts);
            for ( QStringList::Iterator it = lines.begin();
                  it != lines.end(); ++it ) {
                QString& line = *it;
                QStringList cols = line.split("|");

                bool ok = (cols.size() >= 7);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse my_trades line!");
                    continue;
                }

                Order::OrderId orderId = cols[1].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse orderId of my_trades line!");
                    continue;
                }

                int tradesQty = cols[6].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse tradesQty of my_trades line!");
                    continue;
                }

                ADSmartPtr<ADOrderPrivate> orderPtr;

                // Lock
                QWriteLocker wLocker( &m_rwLock );
                // Check orders operations by order id
                QList< ADSmartPtr<ADOrderOperationPrivate> > orderOps;
                if ( _findOperationsByOrderId(orderId, orderOps) ) {
                    // Wake up all operations
                    QList< ADSmartPtr<ADOrderOperationPrivate> >::Iterator it = orderOps.begin();
                    for ( ; it != orderOps.end(); ++it ) {
                        ADSmartPtr<ADOrderOperationPrivate>& op = *it;
                        op->wakeUpAll( Order::ErrorResult );
                        m_ordersOperations->remove(op->getRequestId());
                    }
                }
                Order::State oldState = Order::UnknownState;
                quint32 trades = 0;

                // Check active orders
                if ( m_activeOrders->contains(orderId) ) {
                    // Get order
                    orderPtr = m_activeOrders->value(orderId);

                    // Set status
                    oldState = orderPtr->getOrderState();
                    trades = orderPtr->updateTradesQty( tradesQty );
                    // Check if fully executed
                    if ( orderPtr->isExecutedQty() ) {
                        m_inactiveOrders->insert(orderId, orderPtr);
                        orderPtr->setOrderState( Order::ExecutedState, orderId );
                    }
                }

                // Unlock
                wLocker.unlock();

                // Emit signal for order
                if ( orderPtr.isValid() ) {
                    if ( orderPtr->isExecutedQty() ) {
                        emit onOrderStateChanged( ADConnection::Order(orderPtr),
                                                  oldState,
                                                  Order::ExecutedState );

                        // Iterate over other operations and emit signals
                        QList< ADSmartPtr<ADOrderOperationPrivate> >::Iterator it = orderOps.begin();
                        for ( ; it != orderOps.end(); ++it ) {
                            ADSmartPtr<ADOrderOperationPrivate>& op = *it;
                            emit onOrderOperationResult( ADConnection::Order(orderPtr),
                                                         ADConnection::Order::Operation(op),
                                                         Order::ErrorResult );
                        }
                    }

                    if ( trades > 0 )
                        emit onTrade( ADConnection::Order(orderPtr), trades );
                }
            }
        }
        // Balance (position)
        else if ( it->blockName.contains(ADBlockName::BALANCE) ) {
            QStringList lines = it->blockData.split("\n", QString::SkipEmptyParts);
            for ( QStringList::Iterator it = lines.begin();
                  it != lines.end(); ++it ) {
                QString& line = *it;
                QStringList cols = line.split("|");

                bool ok = (cols.size() >= 35);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse balance line!");
                    continue;
                }

                QString accCode = cols[0];
                QString paperCode = cols[1];
                float realRest = cols[4].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse realRest from balance line!");
                    continue;
                }
                float balancePrice = cols[8].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse balancePrice from balance line!");
                    continue;
                }
                int paperNo = cols[9].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse paperNo from balance line!");
                    continue;
                }
                float varMargin = cols[34].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse varMargin from balance line!");
                    continue;
                }

                Position position(accCode, paperNo, paperCode, static_cast<int>(realRest),
                                  balancePrice, varMargin);

                // Lock
                QWriteLocker wLocker( &m_rwLock );
                m_positions[accCode][paperCode] = position;
                // Unlock
                wLocker.unlock();
                emit onPositionChanged( position.accCode, position.paperCode, position.paperNo );
            }
        }
        // Historical quotes
        else if ( it->blockName.contains(ADBlockName::HIST_QUOTES) ) {
            bool ok = false;
            RequestId reqId = ADBlockName::HIST_QUOTES.cap(2).toInt(&ok);
            if ( ! ok ) {
                qWarning("Wrong block: can't parse request id from historical quotes" );
                continue;
            }

            // Write lock
            QWriteLocker locker( &m_rwLock );
            if ( ! m_requests->contains(reqId) ) {
                qWarning("Can't find registered historical request: %d", reqId);
                continue;
            }
            ADSmartPtr<RequestDataPrivate> reqData = m_requests->take(reqId);

            // Unlock
            locker.unlock();

            QStringList lines = it->blockData.split("\n", QString::SkipEmptyParts);
            QVector<HistoricalQuote> quotes;
            quotes.reserve( lines.size() ) ;
            for ( QStringList::Iterator it = lines.begin();
                  it != lines.end(); ++it ) {
                QString& line = *it;
                QStringList cols = line.split("|");

                ok = (cols.size() >= 7);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse historical quotes line!");
                    continue;
                }

                int paperNo = cols[0].toInt(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse paperNo from historical quotes line!");
                    continue;
                }

                QDateTime dt = QDateTime::fromString(cols[1], "MM/dd/yyyy hh:mm:ss");
                if ( ! ok || ! dt.isValid() ) {
                    qWarning("Wrong block line: can't parse datetime from historical quotes line!");
                    continue;
                }

                float open = cols[2].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse open from historical quotes line!");
                    continue;
                }

                float high = cols[3].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse high from historical quotes line!");
                    continue;
                }

                float low = cols[4].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse low from historical quotes line!");
                    continue;
                }

                float close = cols[5].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse close from historical quotes line!");
                    continue;
                }

                float volume = cols[6].toDouble(&ok);
                if ( ! ok ) {
                    qWarning("Wrong block line: can't parse volume from historical quotes line!");
                    continue;
                }

                HistoricalQuote quote( paperNo, open, high, low, close, volume, dt);
                quotes.append(quote);
            }

            reqData->setState( Request::RequestCompleted );
            Request req;
            req.m_reqData = reqData;

            emit onHistoricalQuotesReceived( req, quotes );
        }
        // Others
        else {
        }

        emit onDataReceived( *it );
    }
}

void ADConnection::storeDataIntoDB ( const QList<DataBlock>& recv )
{
    // Store list of blocks in one transaction
    m_adDB.driver()->beginTransaction();

    QList<DataBlock>::ConstIterator it = recv.begin();
    for ( ; it != recv.end(); ++it ) {
        // For now we do not support empty blocks
        if ( it->blockData.isEmpty() )
            continue;

        QString blockName = it->blockName;
        QRegExp rx("(\\*.*\\*)");
        int rxRes = rx.indexIn(blockName);
        blockName = (rxRes != -1 ? rx.cap(1) : blockName);

        QString filterName;
        QString tableName;
        bool isHistQuotesStream = false;
        QString histQuotes_timeFrameKey = "timeframe";
        int histQuotes_timeFrameVal = 0;

        // Original simple filter
        if ( s_simpleFilters.contains(blockName) ) {
            isHistQuotesStream = false;
            filterName = s_simpleFilters[blockName];
            tableName = AD_DB_PREFIX + filterName.toUpper();
        }
        // For historical quotes we get timeframe from block name
        else if ( it->blockName.contains(ADBlockName::HIST_QUOTES) ) {
            isHistQuotesStream = true;
            filterName = "historical_quotes";
            tableName = AD_DB_PREFIX + filterName.toUpper();
            bool ok = false;
            histQuotes_timeFrameVal = ADBlockName::HIST_QUOTES.cap(1).toInt(&ok);
            if ( ! ok ) {
                qWarning("Block name is incorrect!");
                return;
            }
        }
        else
            return;

        QStringList lines = it->blockData.split("\r\n");

        // Cache DB schema
        if ( m_dbSchema.size() == 0 && ! _sqlGetDBSchema(m_dbSchema) ) {
            qWarning("Can't get DB schema!");
            return;
        }

        // Check table name
        if ( ! m_dbSchema.contains(tableName) ) {
            qWarning("Table '%s' does not exist in DB!", qPrintable(tableName));
            return;
        }

        QStringList& tableFields = m_dbSchema[tableName];

        foreach ( QString line, lines ) {
            QStringList cols = line.split("|");
            // Remove last column, because of trailing |
            cols.removeLast();
            QStringList placeHolders;
            QStringList colsNames;
            QStringList colsNamesEsc;
            QStringList colsValues;

            if ( cols.size() == 1 ) {
                qWarning("Error: single column %s for block %s, will skip!",
                         qPrintable(cols[0]),
                         qPrintable(blockName));
                continue;
            }

            for ( int i = 0, j = 0; j < cols.size() && i < tableFields.size(); ++i ) {
                if ( isHistQuotesStream && tableFields[i] == histQuotes_timeFrameKey )
                    colsValues.append(QString("%1").arg(histQuotes_timeFrameVal));
                else {
                    int idx = j++;
                    if ( cols[idx].isEmpty() )
                        continue;
                    colsValues.append(cols[idx]);
                }

                placeHolders.append("?");
                colsNames.append(tableFields[i]);
                colsNamesEsc.append('"' + tableFields[i] + '"');
            }
            Q_ASSERT(colsNames.size() == colsValues.size());

            QString sql = QString("INTO %1 (%2) VALUES (%3)")
                .arg(tableName)
                .arg(colsNamesEsc.join(", "))
                .arg(placeHolders.join(", "));

            if ( m_adDB.driverName() == "QIBASE" )
                sql = "UPDATE OR INSERT " + sql;
            else if ( m_adDB.driverName() == "QSQLITE" )
                sql = "INSERT OR REPLACE " + sql;
            else if ( m_adDB.driverName() == "QMYSQL" )
                sql += "INSERT " + sql +
                    QString(" ON DUPLICATE KEY UPDATE %1%2").
                        arg(colsNamesEsc.join("=?, ")).
                        arg("=?");
            else {
                qFatal("Unsupported DB driver '%s'",
                       qPrintable(m_adDB.driverName()));
                Q_ASSERT(0);
            }

            QSqlQuery query( m_adDB );
            query.prepare(sql);
            for ( int i = 0; i < colsValues.size(); ++i ) {
                QVariant var;

                // Timestamp
                if ( s_tablesTimestamps.contains(filterName) &&
                     s_tablesTimestamps[filterName].contains(colsNames[i]) ) {
                    QString& timestampVal = colsValues[i];

                    QRegExp dateTime1Rx("^(\\d+\\/\\d+\\/\\d+ \\d+:\\d+:\\d+)$");
                    QRegExp dateTime2Rx("^(\\d+\\/\\d+\\/\\d+ \\d+:\\d+)$");
                    QRegExp dateRx("^(\\d+\\/\\d+\\/\\d+)$");

                    if ( timestampVal.contains(dateTime1Rx) ) {
                        if ( isHistQuotesStream )
                            var = QDateTime::fromString(timestampVal, "MM/dd/yyyy hh:mm:ss");
                        else
                            var = QDateTime::fromString(timestampVal, "dd/MM/yyyy hh:mm:ss");
                    }
                    else if ( timestampVal.contains(dateTime2Rx) )
                        var = QDateTime::fromString(timestampVal, "dd/MM/yyyy hh:mm");
                    else if ( timestampVal.contains(dateRx) )
                        var = QDateTime::fromString(timestampVal, "dd/MM/yyyy");
                    else
                        qWarning("Field '%d' for table '%s' is not TIMESTAMP", i,
                                 qPrintable(tableName));
                }
                // Other
                else
                    var = colsValues[i];

                query.bindValue(i, var);

                // Double bind for mysql
                if ( m_adDB.driverName() == "QMYSQL" )
                    query.bindValue(i + colsValues.size(), var);
            }
            if ( ! query.exec() ) {
                qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
                qWarning("SQL QUERY (block name=%s, cols size=%d): #%s#\n",
                         qPrintable(blockName),
                         cols.size(),
                         qPrintable(sql));
            }

        }
    }

    // Transaction end
    m_adDB.driver()->commitTransaction();
}

void ADConnection::tcpError ( QTcpSocket&,
                              QAbstractSocket::SocketError err )
{
    qWarning("TCP ERR: %d", err);
    m_lastError = SocketError;
    QThread::quit();
}

void ADConnection::tcpStateChanged ( QTcpSocket&, QAbstractSocket::SocketState st )
{
    if ( st == QAbstractSocket::ConnectedState ) {
        // Send auth request
        sendAuthRequest();
    }
}

bool ADConnection::setMinADDelay ()
{
    return writeToSock( "CL_COMMAND|SET_DELAY|0\r\n\r\n" );
}

void ADConnection::getADLastSimpleFilterUpdates ( QHash<QString, int>&  lastUpdates )
{
    foreach ( QString simpleFilter, s_simpleFilters.values() ) {
        QString sql = "SELECT MAX(%1) FROM %2";
        if ( simpleFilter == "papers" )
            sql = sql.arg("\"create_date\"");
        else
            sql = sql.arg("\"i_last_update\"");
        sql = sql.arg(AD_DB_PREFIX + simpleFilter.toUpper());
        QSqlQuery query( m_adDB );
        if ( ! query.exec(sql) ) {
            qWarning("SQL ERROR: %s", qPrintable(query.lastError().text()));
            lastUpdates[simpleFilter] = 0;
        }
        else {
            if ( query.next() )
                lastUpdates[simpleFilter] = query.value(0).toInt();
            else
                lastUpdates[simpleFilter] = 0;
        }
    }
}

bool ADConnection::setADFilters (
                                 const QHash<QString, int>& simpleFilter,
                                 const QHash<QString, QHash<QString, int> >& complexFilter )
{
    //Lock
    QWriteLocker wLocker( &m_rwLock );

    // Set filters
    m_simpleFilter = simpleFilter;
    m_complexFilter = complexFilter;

    return sendADFilters( QSet<QString>::fromList(s_simpleFilters.values()), m_simpleFilter,
                          s_complexFilters, m_complexFilter );
}

bool ADConnection::resendADFilters ()
{
    //Lock
    QWriteLocker wLocker( &m_rwLock );

    return sendADFilters( QSet<QString>::fromList(s_simpleFilters.values()), m_simpleFilter,
                          s_complexFilters, m_complexFilter );
}

bool ADConnection::parseReceivedData ( QList<DataBlock>& outRecv )
{
    QString endBlock( "\r\n\r\n" );

    QString decodedData;
    bool res = readFromSock( decodedData );
    if ( ! res )
        return false;

    m_sockBuffer += decodedData;

    int lastIndx = m_sockBuffer.lastIndexOf( endBlock );
    if ( lastIndx == -1 )
        return false;

    QStringList recv;

    if ( m_sockBuffer.size() - lastIndx  == endBlock.size() ) {
        recv = m_sockBuffer.split( endBlock, QString::SkipEmptyParts );
        m_sockBuffer.clear();
    }
    else {
        int lastStrIndx = m_sockBuffer.size() - lastIndx - endBlock.size();
        recv = m_sockBuffer.left( lastIndx ).split( endBlock );
        m_sockBuffer = m_sockBuffer.right( lastStrIndx );
    }

    if ( recv.size() == 0 ) {
        qWarning("empty data list!");
        return false;
    }

    // Clear
    outRecv.clear();

    for ( int i = 0; i < recv.size(); ++i ) {
        const QString& block = recv[i];
        int indx = block.indexOf( "\r\n" );
        DataBlock adBlock;
        // Block with empty body
        if ( indx == -1 )
            adBlock.blockName = block;
        else {
            int dataIndx = block.size() - indx - 2;
            adBlock.blockName = block.left(indx);
            adBlock.blockData = block.right(dataIndx);
        }
        outRecv.append( adBlock );
    }

#ifdef DO_ALL_LOGGING
    // Log everything
    if ( outRecv.size() > 0 && m_mainLogFile ) {
        QDateTime now = QDateTime::currentDateTime();
        QString str( QString("<<< [%1] Received:\n").arg(now.toString("dd.MM.yyyy hh:mm:ss.zzz")) );
        // Write header in system encoding
        m_mainLogFile->write( str.toLocal8Bit() );
        QList<DataBlock>::Iterator it = outRecv.begin();
        for ( ; it != outRecv.end(); ++it ) {
            // Write block
            DataBlock& db = *it;
            m_mainLogFile->write( db.blockName.toLocal8Bit() );
            m_mainLogFile->write( "\n" );
            m_mainLogFile->write( db.blockData.toLocal8Bit() );
            m_mainLogFile->write( "\n\n" );
        }
        m_mainLogFile->flush();
    }
#endif

    return (outRecv.size() != 0);
}

void ADConnection::sendAuthRequest ()
{
    char connType[ 256 ] = {0};
    int size = sizeof(connType);
    bool res = m_adLib->getConnectionType( connType, &size );
    if ( ! res ) {
        qWarning("call getConnectionType returned false!");
        m_lastError = DynamicLibCallError;
        QThread::quit();
        return;
    }

    QString auth( "auth\r\n" + QString(connType) + "|" + QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm") +
                  "|ADPRC|0|" + m_login + "|" + QString("%1").arg(m_sessInfo.id) +
                  "\r\n\r\n");
    // Write to server in latin1
    writeToSock( auth.toLatin1() );
}

bool ADConnection::parseAuthResponse ( const ADConnection::DataBlock& block )
{
    if ( block.blockName != "authresult")  {
        qWarning("wrong auth block name!");
        return false;
    }

    QStringList authResp = block.blockData.split("|");
    if ( authResp.size() < 4 ) {
        qWarning("wrong auth block data!");
        return false;
    }
    else if ( authResp[3] != "OK" ) {
        qWarning("wrong auth status!");
        return false;
    }

    // Set encoding type in latin1
    m_sessInfo.encType = authResp[0].toLatin1();

    return true;
}

bool ADConnection::writeToSock ( const QByteArray& data )
{
    emit onWriteToSock( data );
    return true;
}

void ADConnection::tcpWriteToSock ( const QByteArray& ba, bool& ret )
{
    Q_ASSERT(QThread::currentThread() == this);

    ret = false;

    // Already closed
    if ( ! m_sock ) {
        qWarning("Warning: write to socket failed, because socket object was "
                 "already closed!");
        return;
    }

#ifdef DO_ALL_LOGGING
    // Log everything
    if ( ba.size() > 0 && m_mainLogFile ) {
        QDateTime now = QDateTime::currentDateTime();
        QString str( QString(">>> [%1] Sent:\n").arg(now.toString("dd.MM.yyyy hh:mm:ss.zzz")) );
        // Write header in system encoding
        m_mainLogFile->write( str.toLocal8Bit() );
        // Write data
        m_mainLogFile->write( ba );
        m_mainLogFile->write( "\n\n" );
        m_mainLogFile->flush();
    }
#endif

    // Feel raw network statistics
    atomic_add64(&m_statTxNet, ba.size());

    char* ptr = 0;
    unsigned int sz = 0;
    bool res = m_adLib->encode( m_sessInfo.encType.data(), m_sessInfo.encType.size(),
                                m_sessInfo.sessionKey.data(), ba.data(), ba.size(),
                                &ptr, &sz );
    if ( ! res ) {
        m_lastError = DynamicLibCallError;
        qWarning("encode failed!");
        return;
    }

    // Feel encoded statistics
    atomic_add64(&m_statTxEncoded, sz);

    // Send
    unsigned int wr = m_sock->write( ptr, sz );
    m_adLib->freeMemory( ptr );

    m_sock->flush();

    if ( wr != sz ) {
        m_lastError = SocketError;
        qWarning("socket write failed!");
        return;
    }

    ret = true;
    return;
}

bool ADConnection::readFromSock ( QString& data )
{
    Q_ASSERT(QThread::currentThread() == this && m_sock);

    QByteArray ba = m_sock->readAll();
    if ( ba.size() == 0 ) {
        qWarning("socket buff is empty!");
        return false;
    }

    // Feel raw network statistics
    atomic_add64(&m_statRxNet, ba.size());

    // Should we decode?
    if ( m_sessInfo.encType.size() == 0 ) {
        // Feel decoded statistics
        atomic_add64(&m_statRxDecoded, ba.size());
        data = m_win1251Codec->toUnicode(ba);
        return true;
    }

    // Concat encoded data
    m_sockEncBuffer += ba;

    char* ptr = 0;
    unsigned int sz = 0;
    unsigned int parsed = 0;
    bool res = m_adLib->decode( m_sessInfo.encType.data(), m_sessInfo.encType.size(),
                                m_sessInfo.sessionKey.data(), m_sockEncBuffer.data(),
                                m_sockEncBuffer.size(), &ptr, &sz, &parsed );
    if ( ! res ) {
        qWarning("decode failed!");
        goto error;
    }
    else if ( ptr == 0 ) {
        // Is not enough to decode. Will try next read
        return false;
    }
    else if ( parsed > (unsigned int)m_sockEncBuffer.size() ) {
        qWarning("decode failed: wrong parsed size: %d", parsed);
        goto error;
    }

    // Feel decoded statistics
    atomic_add64(&m_statRxDecoded, sz);

    // Remove already parsed data
    m_sockEncBuffer = m_sockEncBuffer.right( m_sockEncBuffer.size() - parsed );

    data = m_win1251Codec->toUnicode(ptr, sz);
    m_adLib->freeMemory( ptr );

    return true;

 error:
    m_lastError = DynamicLibCallError;
    m_sockEncBuffer.clear();
    if ( ptr )
        m_adLib->freeMemory( ptr );
    QThread::quit();
    return false;
}

bool ADConnection::sendPing ()
{
    if ( m_state != ConnectedState )
        return false;

    // Resend filters first ping-time (stupid fucking AD)
    if ( ! m_resendFilters ) {
        m_resendFilters = true;
        resendADFilters();
    }

    return writeToSock( "ping\r\n\r\n" );
}

bool ADConnection::sendADFilters (
                                  const QSet<QString>& simpleUpdateKeys,
                                  const QHash<QString, int>& simpleFilter,
                                  const QSet<QString>& complexUpdateKeys,
                                  const QHash<QString, QHash<QString, int> >& complexFilter )
{
    // Create simple filter string
    bool newSimpleFilter = false;
    QString simpleFilterStr;
    {
        QSet<QString>::ConstIterator it = simpleUpdateKeys.begin();
        for ( int i = 0; it != simpleUpdateKeys.end(); ++it, ++i ) {
            newSimpleFilter = true;
            QString filterValue = "=-";
            if ( simpleFilter.contains(*it) ) {
                int val = simpleFilter[ *it ];
                filterValue = QString("=%1").arg(val);
            }
            simpleFilterStr += *it + filterValue +
                (simpleUpdateKeys.size() - 1 == i ? "" : "&");
        }
    }

    // Create complex filter string
    bool newComplexFilter = false;
    QString complexFilterStr;
    {
        QSet<QString>::ConstIterator it = complexUpdateKeys.begin();
        for ( int i = 0; it != complexUpdateKeys.end(); ++it, ++i ) {
            newComplexFilter = true;
            QString filterValue = "=-";
            if ( complexFilter.contains(*it) ) {
                const QHash<QString, int>& valFilter = complexFilter[ *it ];
                QStringList filterValues;
                QHash<QString, int>::ConstIterator it = valFilter.begin();
                for ( ; it != valFilter.end(); ++it ) {
                    const QString& key = it.key();
                    // Do not use value, is always 0
                    filterValues.append( QString("%1|%2").arg(key).arg(0) );
                }
                if ( filterValues.size() > 0 ) {
                    filterValue = QString("=%1").arg(filterValues.join("|"));
                }
            }
            complexFilterStr += *it + filterValue +
                (complexUpdateKeys.size() - 1 == i ? "" : "&");
        }
    }

    if ( newSimpleFilter || newComplexFilter ) {
        QStringList filters ;
        if ( newSimpleFilter )
            filters.append(simpleFilterStr);
        if ( newComplexFilter )
            filters.append(complexFilterStr);

        QString filterStr = QString("SetFilter\r\n%1\r\n\r\n").arg( filters.join("&") );

        // Write to socket in latin1
        return writeToSock( filterStr.toLatin1() );
    }
    else
        // Nothing to do
        return true;
}

/****************************************************************************/
