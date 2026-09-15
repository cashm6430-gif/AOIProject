#pragma once

/**
 * @file ResultJudgeOperator.h
 * @brief 结果判定算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 结果判定算子
         * 汇总所有测量结果，输出最终 OK/NG 判定
         */
        class ResultJudgeOperator : public OperatorBase
        {
        public:
            ResultJudgeOperator()
                : OperatorBase("ResultJudge")
            {
                setDescription("Aggregate measurement results and output final OK/NG judgment");
                setCategory("Output");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["resultKeys"] = QJsonObject{ {"type", "array"}, {"items", QJsonObject{{"type", "string"}}} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "final_result"} };

                schema["properties"] = props;
                return schema;
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QJsonArray keysArray = getParamArray("resultKeys");
                QString outputKey = getParamString("outputKey", "final_result");

                QStringList resultKeys;

                // 如果未指定，自动收集所有 result_ 开头的键
                if (keysArray.isEmpty()) {
                    for (const QString& key : ctx.keys()) {
                        if (key.startsWith("result_")) {
                            resultKeys.append(key);
                        }
                    }
                }
                else {
                    for (const auto& v : keysArray) {
                        resultKeys.append(v.toString());
                    }
                }

                bool allOK = true;
                QStringList ngReasons;

                for (const QString& key : resultKeys) {
                    if (ctx.has(key)) {
                        MeasureResult result = ctx.getMeasureResult(key);
                        if (result.status != 0) {
                            allOK = false;
                            ngReasons.append(QString("%1: %2 (nominal: %3)")
                                .arg(result.name)
                                .arg(result.value)
                                .arg(result.nominalValue));
                        }
                    }
                }

                // 设置最终结果
                ctx.set<bool>("__final_ok__", allOK);
                if (!allOK) {
                    ctx.set<QString>("__final_reason__", ngReasons.join("; "));
                }

                // 也输出为 MeasureResult 格式
                MeasureResult finalResult;
                finalResult.name = "FinalJudgment";
                finalResult.value = allOK ? 1.0 : 0.0;
                finalResult.status = allOK ? 0 : 1;

                ctx.setMeasureResult(outputKey, finalResult);
        return !ctx.hasError();
    }
        };

        // 注册算子
        REGISTER_OPERATOR(ResultJudgeOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
