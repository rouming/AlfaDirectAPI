#ifndef ADOPTION_H
#define ADOPTION_H

#include <QString>
#include <QMap>
#include <QDate>
#include <QDateTime>

#include <ctime>

#ifdef _WIN_
    #include <windows.h>
#else
    #include <sys/time.h>
#endif

//////////////////////////////////////////////////////////////////////

struct ADOption
{
    enum Type {
        Invalid = 0,
        Call,
        Put
    };

    enum PriceType {
        Margin = 0,
        Stock
    };

    ADOption () :
        paperNo(-1),
        basePaperNo(-1),
        strike(0.0),
        type(Invalid),
        priceType(Stock)
    {}

    ADOption ( const QString& code,
               int papNo,
               int basePapNo,
               const QDate& optMatDate,
               float optStrike,
               Type optType,
               PriceType optPriceType ) :
        paperCode(code),
        paperNo(papNo),
        basePaperNo(basePapNo),
        matDate(optMatDate),
        strike(optStrike),
        type(optType),
        priceType(optPriceType)
    {}

    inline bool operator== ( const ADOption& opt ) const
    { return paperNo == opt.paperNo; }

    QString paperCode;
    int paperNo;
    int basePaperNo;
    QDate matDate;

    float strike;
    Type type;
    PriceType priceType;
};

struct ADOptionPair
{
    ADOptionPair () :
        strike(0.0),
        priceType(ADOption::Stock)
    {}

    ADOption optionCall;
    ADOption optionPut;
    float strike;
    ADOption::PriceType priceType;
    QDate matDate;
};

struct ADFutures
{
    ADFutures () :
        paperNo(-1)
    {}

    ADFutures ( const QString& code, int papNo ) :
        paperCode(code),
        paperNo(papNo)
    {}

    // key - mat_date, value - strike, value - price_type, option_pair
    QMap<QDate, QMap<float, QMap<ADOption::PriceType, ADOptionPair> > > options;
    // key - paper_code, value - option
    QMap<QString, ADOption> optionsPlainMap;

    QString paperCode;
    int paperNo;
};


//////////////////////////////////////////////////////////////////////


#endif //ADOPTION_H
