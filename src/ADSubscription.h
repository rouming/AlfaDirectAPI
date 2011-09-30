#ifndef ADSUBSCRIPTION_H
#define ADSUBSCRIPTION_H

#include <QMutex>
#include <QMap>
#include <QWaitCondition>
#include <QLinkedList>
#include <QThread>
#include <QReadWriteLock>

#include "ADConnection.h"

class ADSubscriptionPrivate
{
public:
    ADSubscriptionPrivate ( ADConnection* adConn,
                            const QList<ADConnection::Subscription::Options>& opts );

    ADConnection::Subscription::Result waitForUpdate ();
    bool peekQuote ( int paperNo, ADConnection::Quote& );
    void update ( int paperNo, ADConnection::Subscription::Type );

    const QList<ADConnection::Subscription::Options>& subscriptionOptions () const;

    // This should be called by connection, when connection
    // is going to be destructed, but subscription is in usage.
    void zeroConnectionMember ();

private:
    struct SubscriptionState
    {
        ADConnection::TimeMark lastUpdate;
        ADConnection::TimeMark wakeupTime;
        quint32 minDelay;
        quint32 subscrType;
        bool updated;
        bool appended;
    };

    ADConnection* m_adConnection;
    QList<ADConnection::Subscription::Options> m_opts;
    QReadWriteLock m_rwConnLock;
    QMap<int, SubscriptionState> m_vals;
    QMutex m_mutex;
    QWaitCondition m_wait;
    QLinkedList<int> m_jobs;
    QThread* m_threadCreator;
};

#endif //ADSUBSCRIPTION_H
