#include "ADTemplateParser.h"

/******************************************************************************/

bool ADTemplateParser::addParam ( const QString& key, const QString& val )
{
    if ( m_params.contains(key) )
        return false;
    m_params[key] = val;
    return true;
}

// Do parsing of whole document
QString ADTemplateParser::parse ( const QString& doc ) const
{
    // Resolve all conditional cases
    QString lastResult = resolveCase(doc);
    // Resolve all unconditional cases
    lastResult = resolveParams(lastResult);
    return lastResult;
}

// Try to find block between chars and return it
bool ADTemplateParser::findBlock ( const QString& value,
                                   char begin,
                                   char end,
                                   QString& block ) const

{
    // Init values
    int startOn = 0;
    block = "";

    // Get string for searching
    QString work = value.mid(startOn, value.length() - startOn);

    // Try to find first block char
    int startPos = work.indexOf(begin);
    if ( startPos < 0 )
        return false;

    int endPos = ++startPos;
    int deep = 1;
    int len = work.length();

    // Search end block char while deep is 0 or eof
    while ( endPos < len ) {
        if ( work[endPos] == QChar(end) ) {
            --deep;
            if ( deep == 0 )
                break;
        }
        if ( work[endPos] == QChar(begin) )
            ++deep;
        ++endPos;
    }

    // Ecerything is ok if block is closed
    if ( deep == 0 ) {
        block = work.mid(startPos, endPos - startPos);
        return true;
    }
    else
        return false;
}

// Replace string with some value and return copy of it
QString ADTemplateParser::stringReplace ( const QString& str,
                                          const QString& oldPattern,
                                          const QString& newValue,
                                          int count ) const
{
    QString value = str;
    int index = 0;
    for ( int i = count; i > 0; --i ) {
        index = value.indexOf(oldPattern, index);
        if ( index < 0 )
            break;
        value = value.mid(0, index) +
                newValue +
                value.mid(index + oldPattern.length(),
                          value.length() - index - oldPattern.length());
    }
    return value;
}

// Resolve conditional cases
QString ADTemplateParser::resolveCase ( const QString& value ) const
{
    QString caseStr;
    QString caseOrig;
    QString caseValue;
    QString var;
    QString cond;
    QString condValue;
    QString result = value;

    while ( true ) {
        // Try to find conditional block
        if ( !findBlock(result, '{', '}', caseStr) )
            return result;

        // Restore original block
        caseOrig = "{" + caseStr + "}";
        caseValue = "";

        // Try to find unconditional block in already found block
        if ( !findBlock(caseStr, '<', '>', var) ) {
            result = result.replace(caseOrig, "");
            continue;
        }

        // Get param value
        QString varValue = (!m_params.contains(var) ? "" : m_params[var]);

        // Remove conditional block in already found unconditional one
        caseStr = stringReplace(caseStr, "<" + var + ">", "", 1);

        while ( true ) {
            // Try to find default key in unconditional block
            if ( !findBlock(caseStr, '[', ']', cond) )
                break;

            // Remove found default key in unconditional block
            caseStr = stringReplace(caseStr, "[" + cond + "]", "", 1);

            // Try to find value in unconditional block
            if ( findBlock(caseStr, '[', ']', condValue) ) {
                caseStr = stringReplace(caseStr, "[" + condValue + "]", "", 1);
                // Check found value in block with param value
                if ( cond == varValue ) {
                    caseValue = condValue;
                    break;
                }
            }
            // Safe default value
            else {
                caseValue = cond;
                break;
            }
        }

        // Do final replace
        result = result.replace(caseOrig, caseValue);
    }
}

QString ADTemplateParser::resolveParams ( const QString& value ) const
{
    QString found;
    QString result = value;

    // Do search of all params
    foreach ( QString key, m_params.keys() )
        result = result.replace("<" + key + ">", m_params[key]);

    // Remove all unfound values
    while ( true ) {
        if ( !findBlock(result, '<', '>', found) )
            break;
        result = result.replace("<" + found + ">", "");
    }
    return result;
}

/******************************************************************************/
