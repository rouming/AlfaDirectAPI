#ifndef ADORDER_H
#define ADORDER_H

#include <QMutex>
#include <QMap>
#include <QWaitCondition>
#include <QLinkedList>
#include <QThread>
#include <QReadWriteLock>

#include "ADConnection.h"

class ADOrderPrivate
{
public:
    ADOrderPrivate ();
    ADOrderPrivate ( const QString& accCode,
                     const QString& market,
                     ADConnection::Order::Type,
                     int paperNo, const QString& paperCode,
                     quint32 qty, float price,
                     const QDateTime& dropDt = QDateTime(),
                     ADConnection::Order::OrderId orderId = 0);

    ADConnection::Order::OrderId getOrderId () const;
    void setOrderState ( ADConnection::Order::State state,
                          ADConnection::Order::OrderId orderId );
    void setOrderState ( ADConnection::Order::State state,
                         ADConnection::Order::OrderId orderId,
                         quint32, float );
    void setOrderState ( ADConnection::Order::State state );

    void setPresaveValues ( quint32, float );
    void getPresaveValues ( quint32&, float& );

    bool isExecutedQty () const;
    quint32 getExecutedQty () const;
    quint32 updateRestQty ( quint32 );
    quint32 updateTradesQty ( quint32 );

    QString getAccountCode () const;
    QString getMarket () const;
    ADConnection::Order::State getOrderState () const;
    ADConnection::Order::Type getOrderType () const;
    quint32 getOrderQty () const;
    float getOrderPrice () const;
    QString getOrderPaperCode () const;
    int getOrderPaperNo () const;
    QDateTime getOrderDropDateTime () const;

    void setOrderPaperCode ( const QString& );
    void setMarket ( const QString& );

private:
    quint32 _getExecutedQty () const;

private:
    mutable QMutex m_mutex;
    ADConnection::Order::OrderId m_orderId;
    ADConnection::Order::State m_state;
    ADConnection::Order::Type m_type;
    QString m_accCode;
    QString m_market;
    quint32 m_qty;
    float m_price;
    QString m_paperCode;
    int m_paperNo;
    QDateTime m_dropDt;
    quint32 m_preQty;
    float m_prePrice;
    quint32 m_tradesQty;
    quint32 m_restQty;
};

class ADOrderOperationPrivate
{
public:
    ADOrderOperationPrivate ( ADConnection::RequestId,
                              ADConnection::Order::OperationType,
                              const ADSmartPtr<ADOrderPrivate>& order );
    void wakeUpAll ( ADConnection::Order::OperationResult );

    ADSmartPtr<ADOrderPrivate> getOrder () const;
    ADConnection::Order::OperationType getOperationType () const;
    ADConnection::RequestId getRequestId () const;

    ADConnection::Order::OperationResult waitForOperationResult ( quint32 msecs );
    ADConnection::Order::OperationResult getOperationResult () const;

private:
    mutable QMutex m_mutex;
    ADConnection::RequestId m_reqId;
    ADConnection::Order::OperationType m_type;
    ADConnection::Order::OperationResult m_result;
    QWaitCondition m_wait;
    ADSmartPtr<ADOrderPrivate> m_order;
};

#endif //ADORDER_H
