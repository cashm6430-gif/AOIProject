#pragma once

#include "datadefine.h"
#include <any>
#include <string>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>

struct AlgParams
{
    CommonParams commonParams;
};

// pipeline 中单个步骤
struct PipelineStep
{
    QString id;
    QString op;             // operator
    QJsonObject params;
    QStringList deps;
};

class AlgRecipe
{
public:
    static AlgRecipe& getInstance()
    {
        static AlgRecipe instance;
        return instance;
    }

    void SetRecipe(const QJsonDocument& algDoc);
    QJsonDocument GetRecipe();

    void SetRecipeDir(const QString& recipeDir);

    bool LoadRecipe(const QString& recipeDir, QString currRcp = "MeasureRecipe");
    void SaveRecipe(const QString& recipeDir, QString currRcp = "MeasureRecipe");

    QStringList GetRecipeList();

    void SetAlgParams(const AlgParams& algParams);
    AlgParams GetAlgParams();

    // pipeline 步骤访问
    const QVector<PipelineStep>& GetPipelineSteps() const { return m_pipelineSteps; }
    void SetPipelineSteps(const QVector<PipelineStep>& steps);

private:
    AlgRecipe() {};
    ~AlgRecipe() = default;

    AlgRecipe(const AlgRecipe&) = delete;
    AlgRecipe(AlgRecipe&&) = delete;
    AlgRecipe& operator=(const AlgRecipe&) = delete;
    AlgRecipe& operator=(AlgRecipe&&) = delete;

    void parseJsonDoc();
    void genJsonDoc();

private:
    QJsonDocument m_algDoc;
    AlgParams m_measureParams;
    QString m_recipeDir;
    QString m_recipeName;
    QVector<PipelineStep> m_pipelineSteps;
};