### 2.1 AlgorithmContext

```cpp
// algorithm_context.hpp
#pragma once
#include <QHash>
#include <QVariant>
#include <QString>

class AlgorithmContext {
public:
    template<typename T>
    void set(const QString& key, const T& value) {
        data_[key] = QVariant::fromValue(value);
    }

    template<typename T>
    T get(const QString& key) const {
        return data_.value(key).value<T>();
    }

    bool has(const QString& key) const {
        return data_.contains(key);
    }

private:
    QHash<QString, QVariant> data_;
};
Q_DECLARE_METATYPE(QImage)
Q_DECLARE_METATYPE(PointCloud)
```

### 2.2 IOperator 接口

```cpp
// ioperator.hpp
#pragma once
#include <QString>
#include <QJsonObject>
#include <memory>

class AlgorithmContext;

class IOperator {
public:
    virtual ~IOperator() = default;
    virtual QString name() const = 0;
    virtual void setParams(const QJsonObject& obj) = 0;
    virtual void execute(AlgorithmContext& ctx) = 0;
};
using OperatorPtr = QSharedPointer<IOperator>;
```

### 2.3 示例算子：GaussianBlurOperator

```cpp
// op_gaussian_blur.hpp
#pragma once
#include "ioperator.hpp"
#include "algorithm_context.hpp"

class GaussianBlurOperator : public IOperator {
public:
    QString name() const override { return "GaussianBlur"; }

    void setParams(const QJsonObject& obj) override {
        kernel_ = obj.value("kernel").toInt(5);
        sigma_  = obj.value("sigma").toDouble(1.0);
    }

    void execute(AlgorithmContext& ctx) override {
        QImage img = ctx.get<QImage>("input_image");
        QImage out = applyGaussian(img, kernel_, sigma_); // 你实现
        ctx.set("blur_image", out);
    }

private:
    int kernel_ = 5;
    double sigma_ = 1.0;
};
```

