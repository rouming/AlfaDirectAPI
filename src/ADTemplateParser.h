#ifndef ADTEMPLATEPARSER_H
#define ADTEMPLATEPARSER_H

#include <QHash>

class ADTemplateParser
{
public:
    bool addParam ( const QString& key, const QString& val );
    QString parse ( const QString& doc ) const;

private:
    bool findBlock ( const QString& value,
                     char begin,
                     char end,
                     QString& block ) const;

    QString stringReplace ( const QString& value,
                            const QString& oldPattern,
                            const QString& newValue,
                            int count ) const;

    QString resolveCase ( const QString& value ) const;
    QString resolveParams ( const QString& value ) const;

private:
    QHash<QString, QString> m_params;
};

#endif //ADTEMPLATEPARSER_H
