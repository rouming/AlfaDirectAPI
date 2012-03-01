#include "ADConnection.h"
#include "ADOrder.h"

/****************************************************************************/

ADOrderPrivate::ADOrderPrivate () :
    m_orderId(0),
    m_state(ADConnection::Order::UnknownState),
    m_type(ADConnection::Order::UnknownType),
    m_qty(0),
    m_price(0.0),
    m_paperNo(0),
    m_preQty(0),
    m_prePrice(0.0),
    m_tradesQty(0),
    m_restQty(0)
{}

ADOrderPrivate::ADOrderPrivate ( const QString& accCode,
                                 const QString& market,
                                 ADConnection::Order::Type t,
                                 int paperNo, const QString& paperCode,
                                 quint32 qty,
                                 float price, const QDateTime& dropDt,
                                 ADConnection::Order::OrderId orderId ) :
    m_orderId(orderId),
    m_state(ADConnection::Order::UnknownState),
    m_type(t),
    m_accCode(accCode),
    m_market(market),
    m_qty(qty),
    m_price(price),
    m_paperCode(paperCode),
    m_paperNo(paperNo),
    m_dropDt(dropDt),
    m_preQty(0),
    m_prePrice(0.0),
    m_tradesQty(0),
    m_restQty(qty)
{
    if ( ! m_dropDt.isValid() )
        m_dropDt = QDateTime::currentDateTime().addDays(1);

}

ADConnection::Order::OrderId ADOrderPrivate::getOrderId () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_orderId;
}

void ADOrderPrivate::setOrderState ( ADConnection::Order::State state,
                                     ADConnection::Order::OrderId orderId )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    m_state = state;
    m_orderId = orderId;
}

void ADOrderPrivate::setOrderState ( ADConnection::Order::State state,
                                     ADConnection::Order::OrderId orderId,
                                     quint32 qty, float price )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    m_state = state;
    m_orderId = orderId;
    m_qty = qty;
    m_restQty = qty;
    m_price = price;
}

void ADOrderPrivate::setOrderState ( ADConnection::Order::State state )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    m_state = state;
}

void ADOrderPrivate::setPresaveValues ( quint32 qty, float price )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    m_preQty = qty;
    m_prePrice = price;
}

void ADOrderPrivate::getPresaveValues ( quint32& qty, float& price )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    qty = m_preQty;
    price = m_prePrice;
}

bool ADOrderPrivate::isExecutedQty () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return (m_qty == _getExecutedQty());
}

quint32 ADOrderPrivate::getExecutedQty () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return _getExecutedQty();
}

quint32 ADOrderPrivate::_getExecutedQty () const
{
    Q_ASSERT(m_qty >= m_restQty);
    quint32 restTrades = m_qty - m_restQty;
    return (restTrades > m_tradesQty ? restTrades : m_tradesQty);
}

quint32 ADOrderPrivate::updateRestQty ( quint32 restQty )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    quint32 trades = 0;
    if ( m_restQty > restQty && m_qty > restQty ) {
        m_restQty = restQty;
        quint32 restTrades = m_qty - m_restQty;
        if ( restTrades > m_tradesQty ) {
            trades = restTrades - m_tradesQty;
        }
    }
    return trades;
}

quint32 ADOrderPrivate::updateTradesQty ( quint32 tradesQty )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    quint32 trades = 0;
    if ( m_qty >= (m_tradesQty + tradesQty) ) {
        m_tradesQty += tradesQty;
        quint32 restTrades = m_qty - m_restQty;
        if ( restTrades < m_tradesQty ) {
            trades = m_tradesQty - restTrades;
        }
    }
    return trades;
}

QString ADOrderPrivate::getAccountCode () const
{ return m_accCode; }

QString ADOrderPrivate::getMarket () const
{ return m_market; }

ADConnection::Order::State ADOrderPrivate::getOrderState () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_state;
}

ADConnection::Order::Type ADOrderPrivate::getOrderType () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_type;
}

quint32 ADOrderPrivate::getOrderQty () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_qty;
}

float ADOrderPrivate::getOrderPrice () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_price;
}

QString ADOrderPrivate::getOrderPaperCode () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_paperCode;
}

int ADOrderPrivate::getOrderPaperNo () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_paperNo;
}

QDateTime ADOrderPrivate::getOrderDropDateTime () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_dropDt;
}

void ADOrderPrivate::setOrderPaperCode ( const QString& paperCode )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    m_paperCode = paperCode;
}

void ADOrderPrivate::setMarket ( const QString& market )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    m_market = market;
}

/****************************************************************************/

ADOrderOperationPrivate::ADOrderOperationPrivate ( ADConnection::RequestId reqId,
                                                   ADConnection::Order::OperationType ot,
                                                    const ADSmartPtr<ADOrderPrivate>& order ) :
    m_reqId(reqId),
    m_type(ot),
    m_result(ADConnection::Order::UnknownOperationResult),
    m_order(order)
{}

void ADOrderOperationPrivate::wakeUpAll ( ADConnection::Order::OperationResult opRes )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    if ( m_result != ADConnection::Order::UnknownOperationResult )
        return;
    m_result = opRes;
    m_wait.wakeAll();
}

ADSmartPtr<ADOrderPrivate> ADOrderOperationPrivate::getOrder () const
{ return m_order; }

ADConnection::Order::OperationType ADOrderOperationPrivate::getOperationType () const
{ return m_type; }

ADConnection::RequestId ADOrderOperationPrivate::getRequestId () const
{ return m_reqId; }

ADConnection::Order::OperationResult ADOrderOperationPrivate::waitForOperationResult ( quint32 msecs )
{
    //Lock
    QMutexLocker locker( &m_mutex );
    if ( m_result != ADConnection::Order::UnknownOperationResult )
        return m_result;
    bool res = m_wait.wait( &m_mutex, (msecs == 0 ? ULONG_MAX : msecs) );
    if ( ! res )
        return ADConnection::Order::TimeoutResult;
    return m_result;
}

ADConnection::Order::OperationResult ADOrderOperationPrivate::getOperationResult () const
{
    //Lock
    QMutexLocker locker( &m_mutex );
    return m_result;
}

/****************************************************************************/

ADConnection::Order::Order ()
{}

ADConnection::Order::Order ( const ADSmartPtr<class ADOrderPrivate>& orderPtr ) :
    m_order(orderPtr)
{}

bool ADConnection::Order::isValid () const
{ return m_order.isValid(); }

bool ADConnection::Order::operator == ( const ADConnection::Order& order ) const
{
    if ( m_order.isValid() && order.isValid() )
       return m_order == order.m_order;
    else
        return false;
}

QString ADConnection::Order::getAccountCode () const
{
    if ( m_order.isValid() )
        return m_order->getAccountCode();
    return "";
}

QString ADConnection::Order::getMarket () const
{
    if ( m_order.isValid() )
        return m_order->getMarket();
    return "";
}

ADConnection::Order::State ADConnection::Order::getOrderState () const
{
    if ( m_order.isValid() )
        return m_order->getOrderState();
    return ADConnection::Order::UnknownState;
}

ADConnection::Order::Type ADConnection::Order::getOrderType () const
{
    if ( m_order.isValid() )
        return m_order->getOrderType();
    return ADConnection::Order::UnknownType;
}

quint32 ADConnection::Order::getOrderQty () const
{
    if ( m_order.isValid() )
        return m_order->getOrderQty();
    return 0;
}

float ADConnection::Order::getOrderPrice () const
{
    if ( m_order.isValid() )
        return m_order->getOrderPrice();
    return 0.0;
}

QString ADConnection::Order::getOrderPaperCode () const
{
    if ( m_order.isValid() )
        return m_order->getOrderPaperCode();
    return "";
}

int ADConnection::Order::getOrderPaperNo () const
{
    if ( m_order.isValid() )
        return m_order->getOrderPaperNo();
    return 0;
}

QDateTime ADConnection::Order::getOrderDropDateTime () const
{
    if ( m_order.isValid() )
        return m_order->getOrderDropDateTime();
    return QDateTime();
}

/****************************************************************************/

bool ADConnection::Order::Operation::isValid () const
{ return m_op.isValid(); }

ADConnection::Order::OperationResult ADConnection::Order::Operation::waitForOperationResult (
    quint32 msecs )
{
    if ( m_op.isValid() )
        return m_op->waitForOperationResult(msecs);
    return ADConnection::Order::UnknownOperationResult;
}

ADConnection::Order::OperationResult ADConnection::Order::Operation::getOperationResult () const
{
    if ( m_op.isValid() )
        return m_op->getOperationResult();
    return ADConnection::Order::UnknownOperationResult;
}

ADConnection::Order::OperationType ADConnection::Order::Operation::getOperationType () const
{
    if ( m_op.isValid() )
        return m_op->getOperationType();
    return ADConnection::Order::UnknownOperation;
}

bool ADConnection::Order::Operation::operator == ( const ADConnection::Order::Operation& op ) const
{
    if ( m_op.isValid() && op.isValid() )
        return m_op == op.m_op;
    else
        return false;
}

ADConnection::Order::Operation::Operation ()
{}

ADConnection::Order::Operation::Operation (
    const ADSmartPtr<class ADOrderOperationPrivate>& op ) :
    m_op(op)
{}

/****************************************************************************/
