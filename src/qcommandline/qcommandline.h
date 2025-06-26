#ifndef QCOMMANDLINE_H
#define QCOMMANDLINE_H

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QFlags>

class QCommandLine : public QObject {
    Q_OBJECT
public:
    enum Type {
        Switch = 0x01, /**< argument has no value */
        Param = 0x02 /**< argument has a value */
    };
    Q_DECLARE_FLAGS(Types, Type)

    enum Flag {
        Default = 0x00, /**< default options */
        Optional = 0x01, /**< optional argument */
        Multiple = 0x02, /**< argument can be used multiple times */
        Positional = 0x04 /**< positional argument */
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    struct ConfigEntry {
        const char* name;
        QCommandLine::Type type;
        QCommandLine::Flags flags;
        const char* description;
        const char* valueName;
        const char* defaultValue;
    };

    explicit QCommandLine(QObject* parent = nullptr);

    void setConfig(const ConfigEntry* config);
    void setArguments(const QStringList& arguments);

    bool parse();

    QVariant value(const QString& name) const;
    bool isSet(const QString& name) const;

    QString help(bool full = false) const;
    QString version() const;

signals:
    void optionFound(const QString& name, const QVariant& value);
    void switchFound(const QString& name);
    void paramFound(const QString& name, const QVariant& value);
    void parseError(const QString& errorString);

    void helpRequested();
    void versionRequested();

private:
    const ConfigEntry* m_config;
    QStringList m_arguments;
    QVariantMap m_parsedValues;
    QStringList m_positionalArguments;

    // Helper to find an entry by name
    const ConfigEntry* findConfigEntry(const QString& name) const;
    void emitSignals();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QCommandLine::Types)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCommandLine::Flags)

// This macro must match the struct definition above
#define QCOMMANDLINE_CONFIG_ENTRY_END                                                                                  \
    { nullptr, QCommandLine::Type(0), QCommandLine::Flag(0), nullptr, nullptr, nullptr }

#endif // QCOMMANDLINE_H
