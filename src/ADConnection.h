#ifndef ADCONNECTION_H
#define ADCONNECTION_H

#include <QTcpSocket>
#include <QHash>
#include <QFile>
#include <QString>
#include <QDateTime>
#include <QByteArray>
#include <QThread>
#include <QBuffer>
#include <QMutex>
#include <QWaitCondition>
#include <QReadWriteLock>
#include <QTimer>
#include <QSqlDatabase>

#include "ADSmartPtr.h"
#include "ADLibrary.h"
#include "ADOption.h"

struct ADSessionInfo
{
    ADSessionInfo () :
        id(0), serverPort(0),
        RURtoUSD(0), RURtoEUR(0), userID(0),
        certCtx(0), provCtx(0)
    {}

    QString message;
    int id;
    QByteArray sessionKey;
    QDateTime serverTime;
    QByteArray certData;
    QString nickName;
    QString cspSN;
    QString cspKey;
    QString serverHost;
    int serverPort;
    QString fullName;
    QString accounts;
    double RURtoUSD;
    double RURtoEUR;
    int userID;
    QByteArray encType;

    void* certCtx;
    void* provCtx;
};

class ADConnection : public QThread
{
    Q_OBJECT
public:
    enum State
    {
        ConnectedState = 0,
        DisconnectedState
    };

    enum Error
    {
        NoError = 0,
        BootstrapError,
        Windows1251DoesNotExistError,
        DynamicLibLoadError,
        DynamicLibCallError,
        CertificateError,
        ContextError,
        SocketError,
        SSLSocketError,
        SSLCertificateError,
        ParseHTTPDataError,
        AuthError,
        SQLConnectError,
    };

    class Order
    {
    public:
        enum State
        {
            UnknownState    = 0, /** Order has not been inited */
            AcceptingState  = 1, /** Order is being sent to broker */
            AcceptedState   = 2, /** Order has been accepted by exchange */
            ExecutedState   = 3, /** Order has been executed */
            CancellingState = 4, /** Order is going to be cancelled */
            CancelledState  = 5, /** Order has been cancelled */
            ChangingState   = 6, /** Order is going to be changed */
        };

        enum OperationResult
        {
            UnknownOperationResult = 0,
            SuccessResult,
            ErrorResult,
            TimeoutResult
        };

        enum OperationType
        {
            UnknownOperation = 0,
            CreateOrder,
            ChangeOrder,
            CancelOrder
        };

        class Operation
        {
        public:
            Operation ();
            bool isValid () const;
            Order::OperationResult waitForOperationResult ( quint32 msecs = 0 );
            Order::OperationResult getOperationResult () const;
            Order::OperationType getOperationType () const;

            bool operator== ( const Operation& ) const;

        private:
            friend class ADConnection;
            Operation ( const ADSmartPtr<class ADOrderOperationPrivate>& );

        private:
            ADSmartPtr<class ADOrderOperationPrivate> m_op;
        };

        enum Type
        {
            UnknownType = 0,
            Buy,
            Sell
        };

        typedef quint32 OrderId;

        // Default ctr
        Order ();

        /** Return true if valid */
        bool isValid () const;

        /** Returns account code */
        QString getAccountCode () const;

        /** Returns market */
        QString getMarket () const;

        /** Returns order state */
        State getOrderState () const;

        /** Returns order type */
        Type getOrderType () const;

        /** Returns order qty */
        quint32 getOrderQty () const;

        /** Returns order price */
        float getOrderPrice () const;

        /** Returns order paper code */
        QString getOrderPaperCode () const;

        /** Returns order paper no */
        int getOrderPaperNo () const;

        /** Returns drop timestamp */
        QDateTime getOrderDropDateTime () const;

        bool operator== ( const Order& ) const;

    private:
        friend class ADConnection;
        Order ( const ADSmartPtr<class ADOrderPrivate>& orderPtr );

    private:
        ADSmartPtr<class ADOrderPrivate> m_order;
    };

    struct DataBlock
    {
        QString blockName;
        QString blockData;
    };

    struct BidOffer
    {
        BidOffer ();
        BidOffer ( float p, int b, int s );

        float price;
        int buyersQty;
        int sellersQty;
    };

    struct Quote
    {
        Quote ();

        int paperNo;
        QString paperCode;
        QString market;
        float lastPrice;

        float getBestSeller () const;
        float getBestBuyer () const;

        QString toStringBestSellers ( quint32 num ) const;
        QString toStringBestBuyers ( quint32 num ) const;

        QMap<float, int> buyers;
        QMap<float, int> sellers;
        QMap<float, BidOffer> bidOffers;
    };

    struct HistoricalQuote
    {
        HistoricalQuote ();
        HistoricalQuote ( int, float, float, float, float, float, const QDateTime& );

        int paperNo;
        float open;
        float high;
        float low;
        float close;
        float volume;
        QDateTime dt;
    };

    struct Position
    {
        Position ();
        Position ( const QString& accCode, const QString& market,
                   int paperNo, const QString& paperCode,
                   qint32 qty, float price, float varMargin );

        bool isMoney () const;

        QString accCode;
        QString market;
        int paperNo;
        QString paperCode;
        qint32 qty;
        float price;
        float varMargin;
    };

    class Subscription
    {
    public:
        enum Type {
            QuoteSubscription = 1<<0,
            QueueSubscription = 1<<1
        };

        enum ResultType {
            SuccessResult = 0,
            InterruptResult,
            NotInitedResult,
            WrongThreadResult
        };

        struct Result {
            ResultType resultCode;
            int paperNo;
        };

        class Options {
        public:
            Options ( const QSet<int>&, quint32 subscrType,
                      quint32 subscrTypeReceive,
                      quint32 minDelay = 0 );

        private:
            friend class ADSubscriptionPrivate;
            friend class ADConnection;
            QSet<int> m_paperNos;
            quint32 m_subscrType;
            quint32 m_subscrTypeReceive;
            quint32 m_minDelay;
        };

        Result waitForUpdate ();
        bool peekQuote ( int paperNo, Quote& );
        bool isValid () const;
        operator bool () const;

    private:
        friend class ADConnection;
        ADSmartPtr<class ADSubscriptionPrivate> m_subscr;
    };

    struct LogParam
    {
        bool isFutUpdate;
        QDateTime nowDt;
        ADFutures fut;
        ADConnection::Quote futQuote;
        ADOption opt;
        const ADConnection::Quote optQuote;
        float impl_vol;
        float sell_impl_vol;
        float buy_impl_vol;

        LogParam ( bool updateType, const QDateTime& nowDt,
                   const ADFutures& fut, const ADConnection::Quote& futQuote,
                   const ADOption& opt, const ADConnection::Quote& optQuote,
                   float impl_vol, float sell_impl_vol, float buy_impl_vol );
    };

    /**
     * Time frame values. Synced with AD stream responses,
     * i.e. do not change them!
     */
    enum TimeFrame {
        MIN_1  = 0,
        MIN_5  = 1,
        MIN_10 = 2,
        MIN_15 = 3,
        MIN_30 = 4,
        MIN_60 = 5,
        DAY    = 6,
        WEEK   = 7,
        MONTH  = 8
    };

    typedef quint32 RequestId;
    const static RequestId InvalidRequestId = 0;

    class Request
    {
    public:
        enum State {
            RequestUnknown    = 0,
            RequestInProgress = 1,
            RequestFailed     = 2,
            RequestCompleted  = 3
        };

        RequestId requestId () const;
        State state () const;
        bool wait ( unsigned long timeout = ULONG_MAX );

    private:
        friend class ADConnection;
        ADSmartPtr<class RequestDataPrivate> m_reqData;
    };


    ADConnection ();
    ~ADConnection ();

    /// General client methods
    bool connect ( const QString& userName, const QString& passwd );
    void disconnect ();
    bool isConnected () const;
    bool serverTime ( QDateTime& ) const;
    ADConnection::State state () const;
    ADConnection::Error error () const;
    bool getNetworkStatistics ( quint64& rxNet, quint64& txNet,
                                quint64& rxDecoded, quint64& txEncoded ) const;

    /// Quote info and subscription
    Subscription subscribeToQuotes ( const QList<Subscription::Options>& opts );

    bool requestHistoricalQuotes ( int paperNo, TimeFrame,
                                   const QDateTime& fromDt,
                                   const QDateTime& toDt,
                                   Request& request );

    bool getQuote ( int paperNo, Quote& quote ) const;
    bool getPositions ( QList<Position>& ) const;
    bool getPosition ( const QString& accCode, int paperNo, Position& ) const;
    bool findFutures ( const QString& market,
                       const QString& futCode, ADFutures& fut );
    bool findPaperNo ( const QString& market,
                       const QString& papCode, bool usingArchive, int& paperNo );

    /// Trade operations
    Order::Operation tradePaper ( Order&, const QString& accCode,
                                  Order::Type, int paperNo, quint32 qty, float price );
    Order::Operation cancelOrder ( Order );
    Order::Operation changeOrder ( Order, quint32 qty, float price );

    // DB operations
    bool logQuote ( bool updateType, const QDateTime& nowDt,
                    const ADFutures& fut, const ADConnection::Quote& futQuote,
                    const ADOption& opt, const ADConnection::Quote& optQuote,
                    float impl_vol, float sell_impl_vol, float buy_impl_vol );

    /// Static helper methods
    typedef quint64 TimeMark;

    static quint64 msecsFromEpoch ();
    static void timeMark ( TimeMark& );
    static quint32 msecsDiffTimeMark ( const TimeMark&, const TimeMark& );
    static TimeMark timeMarkAppendMsecs ( const TimeMark&, quint32 msecs );
    static bool stringToTimeFrame ( const QString&, TimeFrame& );

    static bool ADTimeToDateTime ( int lastUpdate, QDateTime& dt );
    static bool dateTimeToADTime ( const QDateTime& dt, int& adTime );

signals:
    void onStateChanged ( ADConnection::State );
    void onDataReceived ( ADConnection::DataBlock );
    void onQuoteReceived ( int paperNo, ADConnection::Subscription::Type );
    void onHistoricalQuotesReceived ( ADConnection::Request, QVector<ADConnection::HistoricalQuote> );
    void onPositionChanged ( QString accCode, int paperNo );
    void onOrderStateChanged ( ADConnection::Order,
                               ADConnection::Order::State oldState,
                               ADConnection::Order::State newState );
    void onOrderOperationResult ( ADConnection::Order,
                                  ADConnection::Order::Operation,
                                  ADConnection::Order::OperationResult );
    void onTrade ( ADConnection::Order, quint32 qty );

    void onWriteToSock ( QByteArray );

    void onFindFutures ( const QString&, const QString&, ADFutures*, bool* );
    void onFindPaperNo ( const QString&, const QString&, bool, int*, bool* );
    void onTradePaper ( ADConnection::Order::Operation op );
    void onCancelOrder ( ADConnection::Order::Operation op );
    void onChangeOrder ( ADConnection::Order::Operation op,
                         quint32 qty, float price );

    void onLogQuote ( ADSmartPtr<ADConnection::LogParam> );

private:
    void run ();
    void storeDataIntoDB ( const QList<DataBlock>& recv );

    bool updateSessionInfo ( const QString& );
    // Do not lock anything!
    bool _getQuote ( int paperNo, Quote& quote ) const;
    bool _unsubscribeToQuote ( const ADSmartPtr<ADSubscriptionPrivate>& );
    bool _findOperationsByOrderId ( ADConnection::Order::OrderId,
                                    QList< ADSmartPtr<ADOrderOperationPrivate> >& ) const;

    // Tcp helpers
    bool parseReceivedData ( QList<DataBlock>& recv );
    void sendAuthRequest ();
    bool parseAuthResponse ( const DataBlock& block );
    bool writeToSock ( const QByteArray& );
    bool readFromSock ( QString& );
    bool sendPing ();
    bool resendADFilters ();
    bool sendADFilters ( const QSet<QString>& simpleUpdateKeys,
                         const QHash<QString, int>& simpleFilter,
                         const QSet<QString>& complexUpdateKeys,
                         const QHash<QString, QHash<QString, int> >& complexFilter );

    // Tcp callbacks
    void tcpReadyRead ( QTcpSocket& );
    void tcpError ( QTcpSocket&, QAbstractSocket::SocketError err );
    void tcpStateChanged ( QTcpSocket&, QAbstractSocket::SocketState st );
    void tcpWriteToSock ( const QByteArray&, bool& );

    // AD methods
    bool setMinADDelay ();
    void getADLastSimpleFilterUpdates ( QHash<QString, int>& simpleFilterUpdates );
    bool setADFilters ( const QHash<QString, int>& simpleFilter =
                        QHash<QString, int>(),
                        const QHash<QString, QHash<QString, int> >& complexFilter =
                        QHash<QString, QHash<QString, int> >() );

    // SQL helpers
    void _sqlFindFutures ( const QString& market,
                           const QString& futCode, ADFutures& fut, bool& found );
    void _sqlFindPaperNo ( const QString& market,
                           const QString& papCode, bool usingArchive, int& paperNo,
                           bool& found );
    bool _sqlFindActiveOrders ( QList< ADSmartPtr<ADOrderPrivate> >& );
    bool _sqlGetCurrentPositions ( QList<Position>& );
    bool _sqlGetDBSchema ( QHash<QString, QStringList>& );
    bool _sqlExecSelect ( const QString& tableName,
                         const QMap<QString, QVariant>& search,
                         QSqlQuery& resQuery ) const;
    bool sqlLogQuote ( bool updateType, const QDateTime& nowDt,
                       const ADFutures& fut, const ADConnection::Quote& futQuote,
                       const ADOption& opt, const ADConnection::Quote& optQuote,
                       float impl_vol, float sell_impl_vol, float buy_impl_vol );

    bool sqlInsertArchivePaper ( int paperNo, const QString& paperCode,
                                 const QString& tsPaperCode, const QString& placeCode,
                                 const QString& placeName, int unused,
                                 const QString& expired, const QString& boardCode,
                                 const QString& atCode, const QDateTime& matDate );

    // Trade helpers
    void tradePaper ( ADConnection::Order::Operation op );

    void cancelOrder ( ADConnection::Order::Operation op );

    void changeOrder ( ADConnection::Order::Operation op,
                       quint32 qty, float price );

    bool getCachedTemplateDocument ( const QString& docName,
                                     QByteArray& doc );


    // For subscription usage
    void lockForRead ();
    void unlock ();

private:
    friend class TcpReceiver;
    friend class SQLReceiver;
    friend class GenericReceiver;
    friend class ADSubscriptionPrivate;

    volatile Error m_lastError;
    volatile State m_state;

    class QTextCodec* m_win1251Codec;
    ADSmartPtr<QFile> m_mainLogFile;
    ADSmartPtr<ADLibrary> m_adLib;
    QSqlDatabase m_adDB;
    QHash<QString, QStringList> m_dbSchema;
    mutable QMutex m_mutex;
    QTcpSocket* m_sock;
    ADSessionInfo m_sessInfo;
    QString m_login;
    QString m_password;
    QString m_sockBuffer;
    QByteArray m_sockEncBuffer;
    bool m_authed;
    bool m_resendFilters;
    // Statistics
    volatile atomic64_t m_statRxNet;
    volatile atomic64_t m_statTxNet;
    volatile atomic64_t m_statRxDecoded;
    volatile atomic64_t m_statTxEncoded;

    /// Data
    mutable QReadWriteLock m_rwLock;
    QDateTime m_srvTime;
    QDateTime m_srvTimeUpdate;
    QHash<int, Quote> m_quotes;
    QHash<QString, QHash<int, Position> > m_positions;
    typedef QPair<ADSmartPtr<ADOrderOperationPrivate>, int> OrderOpWithPhase;
    QHash<RequestId, OrderOpWithPhase>* m_ordersOperations;
    QHash<RequestId, ADSmartPtr<RequestDataPrivate> >* m_requests;
    QHash<Order::OrderId, ADSmartPtr<ADOrderPrivate> >* m_activeOrders;
    // We should keep all cancelled orders because of race on AD server:
    //    -> send #1(req_id) cancellation request on order_id
    //    <- recv #1(req_id) success for order_id [here we store orderId as inactive]
    //    <- recv order_id status: W (order_id is removed)
    //    <- recv order_id status: O (order_id is again active (FUCK!!!)) [here we check that orderId NOT is in inactive]
    //    <- recv order_id status: W (order_id is again removed (FUCK!!!))
    QHash<Order::OrderId, ADSmartPtr<ADOrderPrivate> >* m_inactiveOrders;
    // Global request Id (any messages)
    RequestId m_reqId;

    /// Filters and subscriptions
    /// (also saved by RW lock)
    QList<ADSmartPtr<ADSubscriptionPrivate> >* m_subscriptions;
    // filter_name -> i_last_update
    QHash<QString, int> m_simpleFilter;
    // filter_name -> {code	-> i_last_update}
    QHash<QString, QHash<QString, int> > m_complexFilter;

    // Documents' templates
    QHash<QString, QByteArray> m_docTemplates;
};

////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////

class TcpReceiver : public QObject
{
    Q_OBJECT
public:
    TcpReceiver ( class ADConnection* conn, QTcpSocket& );

public slots:
    void pingTimer ();
    void tcpReadyRead ();
    void tcpError ( QAbstractSocket::SocketError err );
    void tcpStateChanged ( QAbstractSocket::SocketState st );
    void tcpWriteToSock ( QByteArray );

private:
    class ADConnection* m_conn;
    QTcpSocket& m_sock;
};

class SQLReceiver : public QObject
{
    Q_OBJECT
public:
    SQLReceiver ( class ADConnection* conn );

public slots:
    void sqlFindFutures ( const QString&, const QString&, ADFutures*, bool* ret );
    void sqlFindPaperNo ( const QString&, const QString&, bool, int*, bool* ret );
    void sqlLogQuote ( ADSmartPtr<ADConnection::LogParam> );

private:
    class ADConnection* m_conn;
};

class GenericReceiver : public QObject
{
    Q_OBJECT
public:
    GenericReceiver ( class ADConnection* conn );

public slots:
    void onTradePaper ( ADConnection::Order::Operation op );
    void onCancelOrder ( ADConnection::Order::Operation op );
    void onChangeOrder ( ADConnection::Order::Operation op,
                         quint32 qty, float price );

private:
    class ADConnection* m_conn;
};

#include <QSslCertificate>
#include <QSslError>

class QNetworkReply;
class QAuthenticator;

class ADCertificateVerifier : public QObject
{
    Q_OBJECT
public:
    ADCertificateVerifier ( const QString& login,
                            const QString& passwd );
    bool verifyADCert () const;
    void setPeerCert ( const QSslCertificate& );

public slots:
    void onAuthRequired ( QNetworkReply* reply,
                          QAuthenticator* auth );
    void onSslErrors ( QNetworkReply* reply,
                       const QList<QSslError>& list );

private:
    QSslCertificate m_peerCert;
    QString m_login;
    QString m_passwd;
    mutable bool m_peerCertVerified;
};

class RequestDataPrivate
{
public:
    RequestDataPrivate ();

    void setState ( ADConnection::Request::State );
    void setRequestId ( ADConnection::RequestId );
    ADConnection::Request::State state () const;
    ADConnection::RequestId requestId () const;
    bool wait ( unsigned long timeout );

private:
    mutable QMutex m_mutex;
    QWaitCondition m_wait;
    volatile ADConnection::RequestId m_reqId;
    volatile ADConnection::Request::State m_state;
};

// Include for MOC file, to have all order methods!
#include "ADOrder.h"

#endif //ADCONNECTION_H
