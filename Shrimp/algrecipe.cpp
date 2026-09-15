#include "algrecipe.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>

using namespace std;

void AlgRecipe::SetRecipe(const QJsonDocument& algDoc)
{
    m_algDoc = algDoc;
    parseJsonDoc();
}

QJsonDocument AlgRecipe::GetRecipe()
{
    return m_algDoc;
}

void AlgRecipe::SetRecipeDir(const QString& recipeDir)
{
    m_recipeDir = recipeDir;
}

bool AlgRecipe::LoadRecipe(const QString& recipeDir, QString currRcp)
{
    m_recipeDir = recipeDir;
    m_recipeName = currRcp;

    auto recipePath = recipeDir + "/" + currRcp + ".json";
    QFile file(recipePath);
    if (file.open(QIODevice::ReadOnly))
    {
        m_algDoc = QJsonDocument::fromJson(file.readAll());
        file.close();
        parseJsonDoc(); // m_algDoc -> m_measureParams
        return true;
    }
    else
        return false;
}

void AlgRecipe::SaveRecipe(const QString& recipeDir, QString currRcp)
{
    QDir dir(recipeDir);
    if (!dir.exists())
    {
        dir.mkpath(recipeDir);
    }

    auto recipePath = recipeDir + "/" + currRcp + ".json";
    QFile file(recipePath);
    if (file.open(QIODevice::WriteOnly))
    {
        genJsonDoc();
        file.write(m_algDoc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

QStringList AlgRecipe::GetRecipeList()
{
    m_recipeDir = QApplication::applicationDirPath() + "/Recipes";
    QDir dir(m_recipeDir);
    QStringList filters, recipes;
    filters << "*.json";
    QFileInfoList fileinfo = dir.entryInfoList(filters, QDir::Files);
    for (int i = 0; i < fileinfo.size(); i++)
    {
        recipes.append(fileinfo[i].fileName().remove(".json"));
    }

    return recipes;
}

void AlgRecipe::SetAlgParams(const AlgParams& algParams)
{
    m_measureParams = algParams;
}

AlgParams AlgRecipe::GetAlgParams()
{
    return m_measureParams;
}

void AlgRecipe::SetPipelineSteps(const QVector<PipelineStep>& steps)
{
    m_pipelineSteps = steps;
}

// m_algDoc -> m_pipelineSteps
void AlgRecipe::parseJsonDoc()
{
    m_pipelineSteps.clear();
    if (m_algDoc.isNull() || !m_algDoc.isObject())
        return;

    QJsonObject root = m_algDoc.object();
    QJsonArray pipeline = root.value("pipeline").toArray();

    for (const QJsonValue& val : pipeline)
    {
        QJsonObject stepObj = val.toObject();
        PipelineStep step;
        step.id = stepObj.value("id").toString();
        step.op = stepObj.value("operator").toString();
        step.params = stepObj.value("params").toObject();

        QJsonArray depsArr = stepObj.value("deps").toArray();
        for (const QJsonValue& dep : depsArr)
            step.deps.append(dep.toString());

        m_pipelineSteps.append(step);
    }
}

// m_pipelineSteps -> m_algDoc
void AlgRecipe::genJsonDoc()
{
    QJsonArray pipeline;
    for (const PipelineStep& step : m_pipelineSteps)
    {
        QJsonObject stepObj;
        stepObj["id"] = step.id;
        stepObj["operator"] = step.op;
        stepObj["params"] = step.params;

        QJsonArray depsArr;
        for (const QString& dep : step.deps)
            depsArr.append(dep);
        stepObj["deps"] = depsArr;

        pipeline.append(stepObj);
    }

    QJsonObject root;
    root["pipeline"] = pipeline;
    m_algDoc = QJsonDocument(root);
}