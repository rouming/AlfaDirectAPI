#include "ADConnection.h"
#include "ADSubscription.h"

/****************************************************************************/

ADConnection::Subscription::Options::Options (
    const QSet<int>& paperNos,
    quint32 subscrType,
    quint32 subscrTypeReceive,
    quint32 minDelay ) :

    m_paperNos(paperNos),
    m_subscrType(subscrType),
    m_subscrTypeReceive(subscrTypeReceive),
    m_minDelay(minDelay)
{}

/****************************************************************************/

ADConnection::Subscription::Result ADConnection::Subscription::waitForUpdate ()
{
    if ( m_subscr )
        return m_subscr->waitForUpdate();
    qWarning("Invalid subscription!");
    ADConnection::Subscription::Result res = { NotInitedResult, 0 };
    return res;
}

bool ADConnection::Subscription::peekQuote ( int paperNo, ADConnection::Quote& quote )
{
    if ( m_subscr )
        return m_subscr->peekQuote( paperNo, quote );
    qWarning("Invalid subscription!");
    return false;
}

bool ADConnection::Subscription::isValid () const
{
    return m_subscr.isValid();
}


ADConnection::Subscription::operator bool () const
{
    return isValid();
}

/****************************************************************************/

ADSubscriptionPrivate::ADSubscriptionPrivate (
    ADConnection* adConn,
    const QList<ADConnection::Subscription::Options>& opts ) :
    m_adConnection(adConn),
    m_opts(opts),
    m_threadCreator(QThread::currentThread())
{
    ADConnection::TimeMark nowMark = 0;
    ADConnection::timeMark(nowMark);
    for ( int i = 0; i < opts.size(); ++i ) {
        foreach ( int paperNo, opts[i].m_paperNos ) {
            if ( m_vals.contains(paperNo) ) {
                qWarning("PaperNo '%d' already exists! Will skip!", paperNo);
                continue;
            }
            SubscriptionState& subscr = m_vals[paperNo];
            ::memset(&subscr, 0, sizeof(subscr));
            subscr.subscrType = opts[i].m_subscrType;
            if ( opts[i].m_minDelay > 0 ) {
                subscr.minDelay = opts[i].m_minDelay;
                subscr.wakeupTime = ADConnection::timeMarkAppendMsecs(nowMark, subscr.minDelay);
                qWarning("v=%d, MIN DELAY=%d\n", paperNo, subscr.minDelay);
            }
        }
    }
}

ADConnection::Subscription::Result ADSubscriptionPrivate::waitForUpdate ()
{
    ADConnection::Subscription::Result res;
    if ( QThread::currentThread() != m_threadCreator ) {
        qWarning("Subscription must be used from thread, "
                 "where it was created!");
        res.resultCode = ADConnection::Subscription::WrongThreadResult;
        return res;
    }
    // Lock
    QMutexLocker locker(&m_mutex);
iterate:
    unsigned long minWait = ULONG_MAX;
    ADConnection::TimeMark nowMark = 0;
    ADConnection::timeMark(nowMark);
    foreach ( int v, m_vals.keys() ) {
        if ( m_vals[v].wakeupTime <= nowMark ) {
            if ( m_vals[v].updated && ! m_vals[v].appended ) {
                m_vals[v].appended = true;
                m_jobs.append(v);
            }
        }
        else {
            quint32 elapsed = ADConnection::msecsDiffTimeMark(nowMark, m_vals[v].wakeupTime);
            minWait = (elapsed < minWait ? elapsed : minWait);
        }
    }

    if ( m_jobs.size() == 0 ) {
        //XXX static int count = 0;
        //XXX printf("#%d WAIT FOR %d\n", ++count, minWait);
        m_wait.wait( &m_mutex, minWait );
        goto iterate;
    }

    ADConnection::timeMark(nowMark);
    int v = m_jobs.takeFirst();
    Q_ASSERT(m_vals.contains(v));
    m_vals[v].updated = false;
    m_vals[v].appended = false;
    if ( m_vals[v].minDelay != 0 )
        m_vals[v].wakeupTime = ADConnection::timeMarkAppendMsecs(nowMark, m_vals[v].minDelay);
    //XXX	lastUpdate = nowMark - m_vals[v].lastUpdate;

    res.resultCode = ADConnection::Subscription::SuccessResult;
    res.paperNo = v;
    return res;
}

bool ADSubscriptionPrivate::peekQuote ( int paperNo, ADConnection::Quote& quote )
{
    // Lock
    QReadLocker rConnLocker( &m_rwConnLock );
    if ( m_adConnection == 0 ) {
        qWarning("Subscription without connection is incredible!");
        return false;
    }

    bool res = false;
    // Lock connection
    m_adConnection->lockForRead();
    {
        // Lock
        QMutexLocker locker(&m_mutex);
        if ( ! m_vals.contains(paperNo) ) {
            qWarning("Unknown paperNo '%d' for this subcription!", paperNo);
            goto done;
        }
        m_jobs.removeOne(paperNo);
        m_vals[paperNo].updated = false;
        m_vals[paperNo].appended = false;
        // Get quote without any locks
        res = m_adConnection->_getQuote( paperNo, quote );
    }
done:
    // Unlock connection
    m_adConnection->unlock();

    return res;
}

void ADSubscriptionPrivate::zeroConnectionMember ()
{
    // Lock
    QWriteLocker wConnLocker( &m_rwConnLock );
    m_adConnection = 0;
}

void ADSubscriptionPrivate::update (
    int paperNo,
    ADConnection::Subscription::Type subscrType )
{
    // Lock
    QMutexLocker locker(&m_mutex);
    if ( ! m_vals.contains(paperNo) )
        return;
    if ( ! (m_vals[paperNo].subscrType & subscrType) )
        return;

    ADConnection::TimeMark nowMark = 0;
    ADConnection::timeMark(nowMark);
    if ( ! m_vals[paperNo].updated )
        m_vals[paperNo].lastUpdate = nowMark;
    m_vals[paperNo].updated = true;

    if ( m_vals[paperNo].wakeupTime <= nowMark && ! m_vals[paperNo].appended ) {
        m_vals[paperNo].appended = true;
        m_jobs.append(paperNo);
        m_wait.wakeAll();
    }
}

const QList<ADConnection::Subscription::Options>&
ADSubscriptionPrivate::subscriptionOptions () const
{
    return m_opts;
}

/****************************************************************************/
