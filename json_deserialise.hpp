#ifndef JSON_DESERIALISER_H
#define JSON_DESERIALISER_H

#include <cstddef>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <vector>
#include <QFile>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace JsonDeserialise {
    template<typename T>
    struct StringConvertor {
        static constexpr bool value = false;
    };

    template<>
    struct StringConvertor<char*> {
        static constexpr bool value = true;
        static inline char* convert(const QString& str) {
            const auto& src = str.toUtf8();
            char* des = new char[src.length() + 1];
            strncpy(des, src, src.length());
            des[src.length()] = '\0';
            return des;
        }

        static inline QString deconvert(char* const& src) {
            return src;
        }
    };

    template<>
    struct StringConvertor<wchar_t*> {
        static constexpr bool value = true;
        static inline wchar_t* const& convert(const QString& str) {
            wchar_t* des = new wchar_t[str.length() + 1];
            str.toWCharArray(des);
            return des;
        }

        static inline QString deconvert(wchar_t* const& src) {
            return QString::fromWCharArray(src);
        }
    };

    template<>
    struct StringConvertor<QString> {
        static constexpr bool value = true;
        static inline const QString& convert(const QString& str) {
            return str;
        }

        static inline QString deconvert(const QString& src) {
            return src;
        }
    };

    template<>
    struct StringConvertor<std::string> {
        static constexpr bool value = true;
        static inline std::string convert(const QString& str) {
            return str.toStdString();
        }

        static inline QString deconvert(const std::string& src) {
            return QString::fromStdString(src);
        }
    };

    template<>
    struct StringConvertor<QByteArray> {
        static constexpr bool value = true;
        static inline QByteArray convert(const QString& str) {
            return str.toUtf8();
        }

        static inline QString deconvert(const QByteArray& src) {
            return QString::fromUtf8(src);
        }
    };

    template<typename T>
    struct is_string {
        static constexpr bool value = StringConvertor<std::decay_t<T>>::value;
    };

    template<typename T, typename Nullable = std::optional<T>>
    struct NullableHandler {
        static constexpr bool value = false;
        inline static decltype(auto) convert(const T& value) {
            return value;
        }
        constexpr inline static decltype(auto) make_empty() {
            return std::nullopt;
        }
    };

    template<typename T>
    struct NullableHandler<T, T*> {
        static constexpr bool value = true;
        inline static T* convert(const T& value) {
            return new T(value);
        }
        constexpr inline static T* make_empty() {
            return nullptr;
        }
    };

    template<typename T>
    struct is_nullable {
        using Type = void;
        static constexpr bool value = false;
    };

    template<typename T>
    struct is_nullable<T*> {
        using Type = T;
        static constexpr bool value = true;
    };

    template<typename T>
    struct is_nullable<std::optional<T>> {
        using Type = T;
        static constexpr bool value = true;
    };

    template<typename T = void, typename...Args>
    constexpr int count() {
        return std::is_same_v<T, void> ? 0 : 1 + count<Args...>();
    }

    class DeserialisableBase {
    public:
        const bool anonymous = false;
        const QString identifier;
        const enum class AsType : unsigned {
            NonTrivial = 0, LIMITED = 1, NULLABLE = 2, STRING = 4, INTEGER = 8, BOOLEAN = 16, REAL = 32, OBJECT = 64, ARRAY_LIKE = 128
        } as;
    protected:
        DeserialisableBase(AsType _as) : anonymous(true), as(_as) {}
        DeserialisableBase(QString name, AsType _as) : identifier(name), as(_as) {}
        DeserialisableBase(const DeserialisableBase&) = delete;
        DeserialisableBase& operator=(const DeserialisableBase&) = delete;
    public:
        virtual ~DeserialisableBase() {}
        virtual void assign(const QJsonValue& value) = 0;
        virtual QJsonValue to_json() const = 0;
        void append(QJsonObject& parent) const {
            if (anonymous)
                throw std::ios_base::failure("JSON Structure Invalid!");
            parent.insert(identifier, to_json());
        }
        void append(QJsonArray& parent) const {
            if (!anonymous)
                throw std::ios_base::failure("JSON Structure Invalid!");
            parent.append(to_json());
        }
    };

    template<typename T = void, typename...Args>
    constexpr bool isValid() {
        return std::is_same_v<T, void> ? true : std::is_base_of_v<DeserialisableBase, T> && isValid<Args...>();
    }

    template<class...Args>
    class JsonSerialiser {
        const std::enable_if_t<isValid<Args...>(), bool> is_array;
        const DeserialisableBase* value[count<Args...>()];
    private:
        constexpr bool isArray() {
            using AsType = DeserialisableBase::AsType;
            if (count<Args...>() == 1)
                return AsType(unsigned(value[0]->as) & unsigned(AsType::ARRAY_LIKE)) == AsType::ARRAY_LIKE && value[0]->anonymous;
            return false;
        }
    public:
        JsonSerialiser(const Args&...args) : is_array(isArray()), value{ &args... } {}

        void serialise(QString filepath) const {
            QFile file(filepath);
            if (!file.open(QFile::WriteOnly))
                throw std::ios_base::failure("Failed to Open File!");
            file.write(serialise());
            file.close();
        }

        QByteArray serialise(bool compress = false) const {
            if (is_array)
                return serialise_array(compress);
            QJsonDocument json;
            QJsonObject obj;
            for (auto i : value)
                i->append(obj);
            json.setObject(obj);
            return json.toJson(QJsonDocument::JsonFormat(compress));
        }

        QByteArray serialise_array(bool compress = false) const {
            QJsonDocument json;
            QJsonArray array;
            for (auto i : value)
                i->append(array);
            json.setArray(array);
            return json.toJson(QJsonDocument::JsonFormat(compress));
        }

        QJsonValue serialise_to_json() const {
            if (is_array) {
                auto json = QJsonArray();
                for (auto i : value)
                    i->append(json);
                return json;
            }
            auto json = QJsonObject();
            for (auto i : value)
                i->append(json);
            return json;
        }

        QJsonValue serialise_to_file(QString filepath) const {
            const auto data = serialise();
            QFile file(filepath);
            if (!file.open(QFile::WriteOnly))
                throw std::ios_base::failure("Failed to Open File!");
            file.write(data);
            file.close();
        }
    };

    template<class...Args>
    class JsonDeserialiser {
        const std::enable_if_t<isValid<Args...>(), bool> is_array;
        mutable bool delete_after_used = false;
        DeserialisableBase* value[count<Args...>()];
    private:
        constexpr bool isArray() {
            using AsType = DeserialisableBase::AsType;
            if (count<Args...>() == 1)
                return AsType(unsigned(value[0]->as) & unsigned(AsType::ARRAY_LIKE)) == AsType::ARRAY_LIKE && value[0]->anonymous;
            return false;
        }
    public:
        JsonDeserialiser(Args*...args) : is_array(isArray()), value{ args... } {}
        JsonDeserialiser(Args&...args) : is_array(isArray()), value{ &args... } {}
        ~JsonDeserialiser() {
            if (delete_after_used)
                for (auto i : value)
                    delete i;
        }
        void clear() const {
            delete_after_used = true;
        }

        void serialise(QString filepath) const {
            QFile file(filepath);
            if (!file.open(QFile::WriteOnly))
                throw std::ios_base::failure("Failed to Open File!");
            file.write(serialise());
            file.close();
        }

        QByteArray serialise(bool compress = false) const {
            if (is_array)
                return serialise_array(compress);
            QJsonDocument json;
            QJsonObject obj;
            for (auto i : value)
                i->append(obj);
            json.setObject(obj);
            return json.toJson(QJsonDocument::JsonFormat(compress));
        }

        QByteArray serialise_array(bool compress = false) const {
            QJsonDocument json;
            QJsonArray array;
            for (auto i : value)
                i->append(array);
            json.setArray(array);
            return json.toJson(QJsonDocument::JsonFormat(compress));
        }

        QJsonValue serialise_to_json() const {
            if (is_array) {
                auto json = QJsonArray();
                for (auto i : value)
                    i->append(json);
                return json;
            }
            auto json = QJsonObject();
            for (auto i : value)
                i->append(json);
            return json;
        }

        QJsonValue serialise_to_file(QString filepath) const {
            const auto data = serialise();
            QFile file(filepath);
            if (!file.open(QFile::WriteOnly))
                throw std::ios_base::failure("Failed to Open File!");
            file.write(data);
            file.close();
        }

        void deserialiseFile(QString filepath) {
            QFile file(filepath);
            if (!file.open(QFile::ReadOnly))
                throw std::ios_base::failure("Failed to Open File!");
            QJsonParseError parser;
            QJsonDocument data = QJsonDocument::fromJson(file.readAll(), &parser);
            if (parser.error != QJsonParseError::NoError)
                throw std::ios_base::failure("JSON Parsing Failed!");
            if (data.isNull() || data.isEmpty())
                return;
            if (data.isArray()) {
                auto json_array = data.array();
                deserialise_array(json_array);
            }
            else {
                auto json_object = data.object();
                deserialise(json_object);
            }
            file.close();
        }
        void deserialise(const QByteArray& json) {
            QJsonParseError parser;
            QJsonDocument data = QJsonDocument::fromJson(json, &parser);
            if (parser.error != QJsonParseError::NoError)
                throw std::ios_base::failure("JSON Parsing Failed!");
            if (data.isNull() || data.isEmpty())
                return;
            if (is_array) {
                if (data.isArray())
                    return deserialise_array(data.array());
                else
                    throw std::ios_base::failure("JSON Parsing Failed!");
            }
            auto json_object = data.object();
            deserialise(json_object);
        }
        void deserialise(const QJsonObject& object) {
            constexpr int size = count<Args...>();
            int count = 0;
            for (auto i : value) {
                count++;
                if (i->anonymous) {
                    i->assign(object);
                    break;
                }
                else
                    if (object.contains(i->identifier))
                        i->assign(object[i->identifier]);
            }
            if (count != size)
                throw std::ios_base::failure("JSON Structure Incompatible!");
        }
        void deserialise_array(const QJsonArray& json) {
            constexpr int size = count<Args...>();
            if ((unsigned(value[0]->as) & unsigned(DeserialisableBase::AsType::ARRAY_LIKE)) == unsigned(DeserialisableBase::AsType::ARRAY_LIKE) && size == 1) {
                value[0]->assign(json);
                return;
            }
            for (auto i : value)
                if (!i->anonymous)
                    throw std::ios_base::failure("JSON Parsing Failed!");
            int count = 0;
            for (const auto& i : json)
                value[count++]->assign(i);
        }
    };

    class Boolean : public DeserialisableBase {
        bool& value;
    public:
        Boolean(bool& source) : DeserialisableBase(AsType::BOOLEAN), value(source) {}
        Boolean(QString name, bool& source) : DeserialisableBase(name, AsType::BOOLEAN), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (data.isString()) {
                auto str = data.toString().toLower();
                if (str == "true")
                    value = true;
                else if (str == "false")
                    value = false;
                else if (str.isEmpty())
                    value = false;
                throw std::ios_base::failure("Type Unmatch!");
            }
            else if (data.isNull())
                value = false;
            else if (data.isBool())
                value = data.toBool();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return value;
        }
    };

    template<typename T = int, bool sign = true, size_t size = 4>
    class Integer;

    template<typename T>
    class Integer<T, true, 4> : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        Integer(U& source) : DeserialisableBase(AsType::INTEGER), value(source) {}
        Integer(QString name, U& source) : DeserialisableBase(name, AsType::INTEGER), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (data.isString())
                value = data.toString().toInt();
            else if (data.isREAL())
                value = data.toInt();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return value;
        }
    };

    template<typename T>
    class Integer<T, false, 4> : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        Integer(U& source) : DeserialisableBase(AsType::INTEGER), value(source) {}
        Integer(QString name, U& source) : DeserialisableBase(name, AsType::INTEGER), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (data.isString())
                value = data.toString().toUInt();
            else if (data.isREAL())
                value = data.toVariant().toUInt();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return (qint64)value;
        }
    };

    template<typename T = double, size_t size = 4>
    class Real;

    template<>
    class Real<double, 4> : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        Real(U& source) : DeserialisableBase(AsType::REAL), value(source) {}
        Real(QString name, U& source) : DeserialisableBase(name, AsType::REAL), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (data.isString())
                value = data.toString().toREAL();
            else if (data.isREAL())
                value = data.toREAL();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return value;
        }
    };

    template<typename T>
    class String : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        String(U& source) : DeserialisableBase(AsType::STRING), value(source) {}
        String(QString name, U& source) : DeserialisableBase(name, AsType::STRING), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (!data.isString() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            value = StringConvertor<U>::convert(data.toString());
        }
        virtual QJsonValue to_json() const override {
            return StringConvertor<U>::convert(value);
        }
    };

    template<typename T, typename StringType>
    class NullableString : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        constexpr static AsType as = AsType(unsigned(AsType::STRING) & unsigned(AsType::NULLABLE));
        NullableString(U& source) : DeserialisableBase(as), value(source) {}
        NullableString(QString name, U& source) : DeserialisableBase(name, as), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (!data.isString() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            if (!data.isNull()) {
                if (NullableHandler<StringType, T>::value)
                    value = NullableHandler<StringType, T>::convert(StringConvertor<StringType>::convert(data.toString()));
                else
                    value = StringConvertor<StringType>::convert(data.toString());
            }
        }
        virtual QJsonValue to_json() const override {
            return value ? StringConvertor<StringType>::convert(*value) : QJsonValue();
        }
    };

    enum class ArrayPushBackWay {
        Unknown = 0,
        Push_Back = 1,
        Append = 2,
        Insert = 3
    };

    template<typename T, typename Element>
    struct choose_pushback {
        template<typename U, typename V>
        static constexpr ArrayPushBackWay calculate() {
            if (is_pushback<U, V>(nullptr))
                return ArrayPushBackWay::Push_Back;
            else if (is_append<U, V>(nullptr))
                return ArrayPushBackWay::Append;
            else if (is_insert<U, V>(nullptr))
                return ArrayPushBackWay::Insert;
            return ArrayPushBackWay::Unknown;
        }
        template<typename U, typename V, typename = decltype(std::declval<U>().push_back(std::declval<V>()))>
        static constexpr bool is_pushback(int* p) {
            return true;
        }
        template<typename...>
        static constexpr bool is_pushback(...) {
            return false;
        }
        template<typename U, typename V, typename = decltype(std::declval<U>().append(std::declval<V>()))>
        static constexpr bool is_append(int* p) {
            return true;
        }
        template<typename...>
        static constexpr bool is_append(...) {
            return false;
        }
        template<typename U, typename V, typename = decltype(std::declval<U>().insert(std::declval<V>()))>
        static constexpr bool is_insert(int* p) {
            return true;
        }
        template<typename...>
        static constexpr bool is_insert(...) {
            return false;
        }
        static constexpr ArrayPushBackWay value = calculate<T, Element>();
    };

    template<typename T, typename StringType>
    class StringArray : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        constexpr static AsType as = AsType(unsigned(AsType::STRING) | unsigned(AsType::ARRAY_LIKE));
        StringArray(U& source) : DeserialisableBase(as), value(source) {}
        StringArray(QString name, U& source) : DeserialisableBase(name, as), value(source) {}

        virtual std::enable_if_t<bool(choose_pushback<T, StringType>::value)> assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray())
                if constexpr(choose_pushback<T, StringType>::value == ArrayPushBackWay::Push_Back)
                    value.push_back(StringConvertor<StringType>::convert(i.toString()));
                else if constexpr(choose_pushback<T, StringType>::value == ArrayPushBackWay::Append)
                    value.append(StringConvertor<StringType>::convert(i.toString()));
                else if constexpr(choose_pushback<T, StringType>::value == ArrayPushBackWay::Insert)
                    value.insert(StringConvertor<StringType>::convert(i.toString()));
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value)
                array.append(StringConvertor<StringType>::convert(i));
            return array;
        }
    };

    template<typename T, typename NullableStringType, typename StringType>
    class NullableStringArray : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        constexpr static AsType as = AsType(unsigned(AsType::NULLABLE) | unsigned(AsType::STRING) | unsigned(AsType::ARRAY_LIKE));
        NullableStringArray(U& source) : DeserialisableBase(as), value(source) {}
        NullableStringArray(QString name, U& source) : DeserialisableBase(name, as), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray())
                if (!i.isNull())
                    value.push_back(NullableHandler<NullableStringType, StringType>::convert(StringConvertor<StringType>::convert(i.toString())));
                else
                    value.push_back(NullableHandler<NullableStringType, StringType>::make_empty());
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value)
                if (i)
                    array.append(StringConvertor<StringType>::convert(*i));
            return array;
        }
    };

    template<typename T, typename StringType, int N>
    class LimitedStringArray : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        constexpr static AsType as = AsType(unsigned(AsType::STRING) | unsigned(AsType::ARRAY_LIKE) | unsigned(AsType::LIMITED));
        LimitedStringArray(U& source) : DeserialisableBase(as), value(source) {}
        LimitedStringArray(QString name, U& source) : DeserialisableBase(name, as), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            int count = 0;
            for (const auto& i : data.toArray()) {
                if (count > N - 1)
                    throw std::ios_base::failure("Array Out of Range!");
                value[count++] = StringConvertor<StringType>::convert(i.toString());
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                QString str = StringConvertor<StringType>::convert(i);
                if (!str.isEmpty())
                    array.append(str);
            }
            return array;
        }
    };

    template<typename T, typename NullableStringType, typename StringType, int N>
    class LimitedNullableStringArray : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        constexpr static AsType as = AsType(unsigned(AsType::NULLABLE) | unsigned(AsType::STRING) | unsigned(AsType::ARRAY_LIKE) | unsigned(AsType::LIMITED));
        LimitedNullableStringArray(U& source) : DeserialisableBase(as), value(source) {}
        LimitedNullableStringArray(QString name, U& source) : DeserialisableBase(name, as), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            int count = 0;
            for (const auto& i : data.toArray()) {
                if (count > N - 1)
                    throw std::ios_base::failure("Array Out of Range!");
                if (!i.isNull())
                    value[count++] = NullableHandler<NullableStringType, StringType>::convert(StringConvertor<StringType>::convert(i.toString()));
                else
                    value[count++] = NullableHandler<NullableStringType, StringType>::make_empty();
            }
        }
        virtual QJsonValue to_json() const {
            QJsonArray array;
            for (const auto& i : value) {
                if (i && !i->isEmpty())
                    array.append(*i);
            }
            return array;
        }
    };

    //Primary Template
    template <typename Any, bool isArray = false, int size = -1, typename TypeInArray = void, int isNullable = -1, typename TypeInNullable = void, int isString = -1>
    struct _Deserialisable;

    template<typename Any, bool isArray = false, int size = -1, typename TypeInArray = void, int isNullable = -1, typename TypeInNullable = void, int isString = -1>
    using _DeserialisableType = typename _Deserialisable<std::decay_t<Any>, isArray, size, TypeInArray, isNullable, TypeInNullable, isString>::Type;

    template<typename Any>
    struct Deserialisable {
        using Type = _DeserialisableType<Any>;
    };

    template<typename Any>
    using DeserialisableType = typename Deserialisable<std::decay_t<Any>>::Type;

    template<class T, typename...WrappedMembers>
    class Object : public DeserialisableBase {
        JsonDeserialiser<WrappedMembers...> deserialiser;
    public:
        constexpr static AsType as = AsType::OBJECT;
        Object(T* _, WrappedMembers&...members) : DeserialisableBase(as), deserialiser(&members...) {}
        Object(T* _, WrappedMembers*...members) : DeserialisableBase(as), deserialiser(members...) {}
        Object(QString name, T* _, WrappedMembers&...members) : DeserialisableBase(name, as), deserialiser(&members...) {}
        Object(QString name, T* _, WrappedMembers*...members) : DeserialisableBase(name, as), deserialiser(members...) {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isObject() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            if (!data.isNull())
                deserialiser.deserialise(data.toObject());
        }
        virtual QJsonValue to_json() const override {
            return deserialiser.serialise_to_json();
        }
        inline void clear() const {
            deserialiser.clear();
        }
    };

    template<typename T>
    struct Info {
        const char* name;
        T* ptr;
        Info(const char* id, T* p) : name(id), ptr(p) {}
    };

    template<typename T, class ObjectType, typename...Members>
    class ObjectArray : public DeserialisableBase {
        using Prototype = Object<ObjectType, DeserialisableType<Members>...>;
        T& value;
        static constexpr size_t size = count<Members...>();
        size_t offset[size];
        const char* names[size];
    public:
        constexpr static AsType as = AsType(unsigned(AsType::ARRAY_LIKE) | unsigned(AsType::OBJECT));
        ObjectArray(T& source, ObjectType* object_ptr, Info<Members>&&...members) : DeserialisableBase(as), value(source), offset{ reinterpret_cast<size_t>(members.ptr)... }, names{ members.name... } {}
        ObjectArray(QString name, T& source, ObjectType* object_ptr, Info<Members>&&...members) : DeserialisableBase(name, as), value(source), offset{ reinterpret_cast<size_t>(members.ptr)... }, names{ members.name... } {}

        virtual std::enable_if_t<bool(choose_pushback<T, ObjectType>::value)> assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                if constexpr(choose_pushback<T, ObjectType>::value == ArrayPushBackWay::Push_Back)
                    value.push_back(ObjectType());
                else if constexpr(choose_pushback<T, ObjectType>::value == ArrayPushBackWay::Append)
                    value.append(ObjectType());
                else if constexpr(choose_pushback<T, ObjectType>::value == ArrayPushBackWay::Insert)
                    value.insert(ObjectType());
                auto ptr = &value.back();
                int nameCount = size, dataCount = size;
                Prototype deserialiser((ObjectType*)nullptr, new DeserialisableType<Members>(names[--nameCount], *reinterpret_cast<Members*>(reinterpret_cast<uint8_t*>(ptr) + offset[--dataCount]))...);
                deserialiser.assign(i);
                deserialiser.clear();
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                const auto ptr = &i;
                int nameCount = size, dataCount = size;
                const Prototype serialiser((ObjectType*)nullptr, new DeserialisableType<Members>(names[--nameCount], const_cast<Members&>(*reinterpret_cast<const Members*>(reinterpret_cast<const uint8_t*>(ptr) + offset[--dataCount])))...);
                array.append(serialiser.to_json());
                serialiser.clear();
            }
            return array;
        }
    };

    template<typename T, typename TrivialType>
    class Array : public DeserialisableBase {
        using U = std::decay_t<T>;
        using Prototype = DeserialisableType<TrivialType>;
        U& value;
    public:
        Array(U& source) : DeserialisableBase(AsType::ARRAY_LIKE), value(source) {}
        Array(QString name, U& source) : DeserialisableBase(name, AsType::ARRAY_LIKE), value(source) {}

        virtual std::enable_if_t<bool(choose_pushback<T, TrivialType>::value)> assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                if constexpr(choose_pushback<T, TrivialType>::value == ArrayPushBackWay::Push_Back)
                    value.push_back(TrivialType());
                else if constexpr(choose_pushback<T, TrivialType>::value == ArrayPushBackWay::Append)
                    value.append(TrivialType());
                else if constexpr(choose_pushback<T, TrivialType>::value == ArrayPushBackWay::Insert)
                    value.insert(TrivialType());
                Prototype deserialiser(value.back());
                deserialiser.assign(i);
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                const Prototype deserialiser(const_cast<TrivialType&>(i));
                array.append(deserialiser.to_json());
            }
            return array;
        }
    };

    template<typename T, typename TypeInNullable>
    class Nullable : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        Nullable(U& source) : DeserialisableBase(AsType::NULLABLE), value(source) {}
        Nullable(QString name, T& source) : DeserialisableBase(name, AsType::NULLABLE), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (data.isNull()) {
                value = NullableHandler<TypeInNullable, U>::make_empty();
                return;
            }
            TypeInNullable tmp;
            DeserialisableType<TypeInNullable> serialiser(tmp);
            serialiser.assign(data);
            value = NullableHandler<TypeInNullable, U>::convert(tmp);
        }
        virtual QJsonValue to_json() const override {
            QJsonValue result;
            if (value)
                result = DeserialisableType<TypeInNullable>(*value).to_json();
            return result;
        }
    };

    template<typename T, typename As>
    class NonTrivial : public DeserialisableBase {
        using U = std::decay_t<T>;
        DeserialisableType<As> value;
    public:
        NonTrivial(U& source) : DeserialisableBase(AsType::NonTrivial), value(reinterpret_cast<As&>(source)) {}
        NonTrivial(QString name, U& source) : DeserialisableBase(name, AsType::NonTrivial), value(name, reinterpret_cast<As&>(source)) {}
        virtual void assign(const QJsonValue& data) override {
            value.assign(data);
        }
        virtual QJsonValue to_json() const override {
            return value.to_json();
        }
    };

    template<typename T, const char* json_name, size_t member_offset>
    struct ReinforcedInfo {
        using Type = T;
        static constexpr const char* name = json_name;
        static constexpr size_t offset = member_offset;
    };

    template<typename ObjectType, typename...MemberInfo>
    class DeserialisableObject : public DeserialisableBase {
        Object<ObjectType, DeserialisableType<typename MemberInfo::Type>...> serialiser;
    public:
        DeserialisableObject(QString json_name, ObjectType& value) : DeserialisableBase(json_name, AsType::OBJECT), serialiser(json_name, &value, new DeserialisableType<typename MemberInfo::Type>(MemberInfo::name, (typename MemberInfo::Type&)*((uint8_t*)(&value) + MemberInfo::offset))...) {}
        DeserialisableObject(ObjectType& value) : DeserialisableBase(AsType::OBJECT), serialiser(&value, new DeserialisableType<typename MemberInfo::Type>(MemberInfo::name, (typename MemberInfo::Type&)*((uint8_t*)(&value) + MemberInfo::offset))...) {}
        virtual void assign(const QJsonValue& data) override {
            serialiser.assign(data);
        }
        virtual QJsonValue to_json() const override {
            return serialiser.to_json();
        }
        ~DeserialisableObject() {
            serialiser.clear();
        }
    };

    template<typename T>
    class SelfDeserialisableObject : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
    public:
        constexpr static AsType as = AsType::OBJECT;
        SelfDeserialisableObject(U& source) : DeserialisableBase(as), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            if (!data.isObject() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            if (data.isObject())
                value.from_json(data.toObject());
        }
        virtual QJsonValue to_json() const override {
            return value.to_json();
        }
    };

    template<typename T, typename KeyType, typename ValueType, const char* KeyName = nullptr>
    class MapArray : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
        QString key = KeyName;
    public:
        MapArray(U& source) : DeserialisableBase(AsType(unsigned(AsType(AsType::ARRAY_LIKE)) | unsigned(AsType(AsType::OBJECT)))), value(source) {}
        virtual void assign(const QJsonValue& data) override {
            for (const auto& i : data.toArray()) {
                if (!data.isArray() && !data.isNull())
                    throw std::ios_base::failure("Type Unmatch!");
                KeyType key_value;
                ValueType value_value;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                key_deserialiser.assign(i.toObject()[key]);
                value_deserialiser.assign(i);
                value[key_value] = value_value;
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto&[key_, value_] : value) {
                KeyType key_value = key_;
                ValueType value_value = value_;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                QJsonObject obj = value_deserialiser.to_json().toObject();
                obj.insert(key, key_deserialiser.to_json());
                array.append(obj);
            }
            return array;
        }
    };

    template<typename T, typename KeyType, typename ValueType>
    class MapArray<T, KeyType, ValueType, nullptr> : public DeserialisableBase {
        using U = std::decay_t<T>;
        U& value;
        QString key;
        std::optional<QString> val_name;
    public:
        MapArray(U& source, QString key_name) : DeserialisableBase(AsType(unsigned(AsType::ARRAY_LIKE) | unsigned(AsType::OBJECT))), value(source), key(key_name) {}
        MapArray(U& source, QString key_name, QString val_json_name) : DeserialisableBase(AsType(unsigned(AsType::ARRAY_LIKE) | unsigned(AsType::OBJECT))), value(source), key(key_name), val_name(val_json_name) {}
        virtual void assign(const QJsonValue& data) override {
            for (const auto& i : data.toArray()) {
                if (!data.isArray() && !data.isNull())
                    throw std::ios_base::failure("Type Unmatch!");
                KeyType key_value;
                ValueType value_value;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                key_deserialiser.assign(i.toObject()[key]);
                if (val_name)
                    value_deserialiser.assign(i.toObject()[*val_name]);
                else
                    value_deserialiser.assign(i);
                value[key_value] = value_value;
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto&[key_, value_] : value) {
                KeyType key_value = key_;
                ValueType value_value = value_;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                QJsonObject obj;
                if (val_name)
                    obj = value_deserialiser.to_json().toObject();
                else
                    obj.insert(*val_name, value_deserialiser.to_json());
                obj.insert(key, key_deserialiser.to_json());
                array.append(obj);
            }
            return array;
        }
    };

    template<class Base, class T, typename...MemberInfo>
    class DerivedObject : public DeserialisableBase {
        Object<T, DeserialisableType<typename MemberInfo::Type>...> serialiser;
        std::enable_if_t<std::is_base_of_v<Base, T>, DeserialisableType<Base>> base_serialiser;
    public:
        DerivedObject(QString json_name, T& value) : DeserialisableBase(json_name, AsType::OBJECT), serialiser(json_name, &value, new DeserialisableType<typename MemberInfo::Type>(MemberInfo::name, (typename MemberInfo::Type&)*((uint8_t*)(&value) + MemberInfo::offset))...), base_serialiser(dynamic_cast<Base&>(value)) {}
        DerivedObject(T& value) : DeserialisableBase(AsType::OBJECT), serialiser(&value, new DeserialisableType<typename MemberInfo::Type>(MemberInfo::name, (typename MemberInfo::Type&)*((uint8_t*)(&value) + MemberInfo::offset))...), base_serialiser(dynamic_cast<Base&>(value)) {}
        virtual void assign(const QJsonValue& data) override {
            serialiser.assign(data);
            base_serialiser.assign(data);
        }
        virtual QJsonValue to_json() const override {
            QJsonObject result = serialiser.to_json().toObject();
            QJsonObject base = base_serialiser.to_json().toObject();
            for (auto i = base.begin(); i != base.end(); i++)
                result.insert(i.key(), i.value());
            return result;
        }
        ~DerivedObject() {
            serialiser.clear();
        }
    };

    template<typename Any>
    struct _Deserialisable<Any, false, -1, void, -1, void, -1> {
        using Type = _DeserialisableType<Any, false, -1, void, is_nullable<Any>::value, typename is_nullable<Any>::Type, -1>;
    };

    template<typename Any>
    struct _Deserialisable<Any, false, -1, void, false, void, -1> {
        using Type = _DeserialisableType<Any, false, -1, void, false, void, is_string<Any>::value>;
    };

    template<typename T, typename TypeInNullable>
    struct _Deserialisable<T, false, -1, void, true, TypeInNullable, -1> {
        using Type = _DeserialisableType<T, false, -1, void, true, TypeInNullable, is_string<TypeInNullable>::value>;
    };

    template<typename T, typename TypeInArray>
    struct _Deserialisable<T, true, -1, TypeInArray, -1, void, -1> {
        using Type = _DeserialisableType<T, true, -1, TypeInArray, is_nullable<TypeInArray>::value, typename is_nullable<TypeInArray>::Type, -1>;
    };

    template<typename T, typename TypeInArray>
    struct _Deserialisable<T, true, -1, TypeInArray, false, void, -1> {
        using Type = _DeserialisableType<T, true, -1, TypeInArray, false, void, is_string<TypeInArray>::value>;
    };

    template<typename T, typename TypeInArray, typename TypeInNullable>
    struct _Deserialisable<T, true, -1, TypeInArray, true, TypeInNullable, -1> {
        using Type = _DeserialisableType<T, true, -1, TypeInArray, true, TypeInNullable, is_string<TypeInNullable>::value>;
    };

    template<typename StringType>
    struct _Deserialisable<StringType, false, -1, void, false, void, true> {
        using Type = String<StringType>;
    };

    template<typename T, typename StringType>
    struct _Deserialisable<T, false, -1, void, true, StringType, true> {
        using Type = NullableString<T, StringType>;
    };

    template<typename T, typename TypeInNullable>
    struct _Deserialisable<T, false, -1, void, true, TypeInNullable, false> {
        using Type = Nullable<T, TypeInNullable>;
    };

    template<typename T>
    struct Deserialisable<std::vector<T>> {
        using Type = _DeserialisableType<std::vector<T>, true, -1, T>;
    };

    template<typename T>
    struct Deserialisable<QList<T>> {
        using Type = _DeserialisableType<QList<T>, true, -1, T>;
    };

    template<typename T>
    struct Deserialisable<std::list<T>> {
        using Type = _DeserialisableType<std::list<T>, true, -1, T>;
    };

    template<typename T>
    struct Deserialisable<std::set<T>> {
        using Type = _DeserialisableType<std::set<T>, true, -1, T>;
    };

    template<typename T>
    struct Deserialisable<QSet<T>> {
        using Type = _DeserialisableType<QSet<T>, true, -1, T>;
    };

    template<typename TypeInArray, int N>
    struct Deserialisable<TypeInArray[N]> {
        using Type = _DeserialisableType<TypeInArray[N], true, N, TypeInArray>;
    };

    template<typename TypeInArray, int N>
    struct Deserialisable <std::array<TypeInArray, N>> {
        using Type = _DeserialisableType<std::array<TypeInArray, N>, true, N, TypeInArray>;
    };

    template<typename T, typename StringType>
    struct _Deserialisable<T, true, -1, StringType, false, void, true> {
        using Type = StringArray<T, StringType>;
    };

    template<typename T, typename Nullable, typename StringType>
    struct _Deserialisable<T, true, -1, Nullable, true, StringType, true> {
        using Type = NullableStringArray<T, Nullable, StringType>;
    };

    template<typename ArrayType, typename T>
    struct _Deserialisable<ArrayType, true, -1, T, false, void, false> {
        using Type = Array<ArrayType, T>;
    };

    template<typename T, typename StringType, int N>
    struct _Deserialisable<T, true, N, StringType, false, void, true> {
        using Type = LimitedStringArray<T, StringType, N>;
    };

    template<typename T, typename Nullable, typename StringType, int N>
    struct _Deserialisable<T, true, N, Nullable, true, StringType, true> {
        using Type = LimitedNullableStringArray<T, Nullable, StringType, N>;
    };

    template<typename Key, typename Value>
    struct Deserialisable<std::map<Key, Value>> {
        using Type = MapArray<std::map<Key, Value>, Key, Value>;
    };

    template<typename Key, typename Value>
    struct Deserialisable<QMap<Key, Value>> {
        using Type = MapArray<QMap<Key, Value>, Key, Value>;
    };

    template<>
    struct Deserialisable<bool> {
        using Type = Boolean;
    };

    template<>
    struct Deserialisable<int> {
        using Type = Integer<int>;
    };      

    template<>
    struct Deserialisable<unsigned> {
        using Type = Integer<unsigned, false>;
    };

    template<>
    struct Deserialisable<REAL> {
        using Type = Real<REAL>;
    };
}

#define declare_top_deserialiser(data_name, var_name) JsonDeserialise::DeserialisableType<decltype(data_name)> var_name((data_name));
#define declare_deserialiser(json_name, data_name, var_name) JsonDeserialise::DeserialisableType<decltype(data_name)> var_name((json_name), (data_name));
#define declare_simple_map_deserialiser(data_name, key_name, val_name, var_name) JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(data_name, key_name, val_name);
#define declare_object_map_deserialiser(data_name, key_name, var_name) JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(data_name, key_name);
#define declare_serialiser(json_name, data_name, var_name) const JsonDeserialise::DeserialisableType<decltype(data_name)> var_name((json_name), const_cast<std::decay_t<decltype(data_name)>&>(data_name));
#define declare_object_deserialiser(json_name, object_type, var_name, ...) JsonDeserialise::Object var_name(QStringLiteral(json_name), (object_type*)(nullptr), __VA_ARGS__);
#define array_object_member(object_type, json_name, member_name) JsonDeserialise::Info(json_name, &((object_type*)nullptr)->member_name)
#define declare_object_array(object_type, data_name, var_name, ...) JsonDeserialise::ObjectArray var_name(data_name, (object_type*)nullptr, __VA_ARGS__);
#define declare_non_trivial_as(type_name, as) template<> struct JsonDeserialise::Deserialisable<type_name> { using Type = JsonDeserialise::NonTrivial<type_name, as>; };
#define register_object_member(object_type, json_name, member_name) namespace JsonDeserialise {const char object_type##_##member_name[] = json_name;};
#define object_member(object_type, member_name) JsonDeserialise::ReinforcedInfo<decltype(((object_type*)nullptr)->member_name), JsonDeserialise::object_type##_##member_name, (offsetof(object_type, member_name))>
#define declare_object(object_type, ...) template<> struct JsonDeserialise::Deserialisable<object_type> { using Type = DeserialisableObject<object_type, __VA_ARGS__>; };
#define declare_class_with_json_constructor_and_serialiser(object_type) template<> struct JsonDeserialise::Deserialisable<object_type> { using Type = SelfDeserialisableObject<object_type>; };
#define declare_map_with_key(container_type, key_type, value_type, key_name) const char key_type##_##value_name[] = key_name; template<> struct JsonDeserialise::Deserialisable<container_type<key_type, value_type>> { using Type = MapArray<container_type<key_type, value_type>, key_type, value_type, key_type##_##value_name>; };
#define declare_object_with_base_class(object_type, base_type, ...) template<> struct JsonDeserialise::Deserialisable<object_type> { using Type = DerivedObject<base_type, object_type, __VA_ARGS__>; };

#endif // JSON_DESERIALISER_H