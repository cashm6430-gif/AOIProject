#pragma once

#include "spdlog/details/null_mutex.h"
#include "spdlog/details/synchronous_factory.h"
#include "spdlog/sinks/base_sink.h"
#include <mutex>
#include <QColor>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>
#include <string>

namespace spdlog {
    namespace sinks {
        /**
         * @brief Qt TextEdit Sink - 兼容 Qt 5.9
         *
         * 支持将日志输出到 QTextEdit 或 QPlainTextEdit
         * 支持颜色高亮
         * 线程安全
         */
        template<typename Mutex>
        class qt_textedit_sink : public base_sink<Mutex>
        {
        public:
            /**
             * @brief 构造函数
             * @param text_edit QTextEdit 指针
             * @param max_lines 最大行数，0 表示不限制
             * @param auto_scroll 是否自动滚动到底部
             * @param color_enabled 是否启用颜色
             */
            qt_textedit_sink(QTextEdit* text_edit,
                std::size_t max_lines = 1000,
                bool auto_scroll = true,
                bool color_enabled = true)
                : text_edit_(text_edit)
                , plain_text_edit_(nullptr)
                , max_lines_(max_lines)
                , auto_scroll_(auto_scroll)
                , color_enabled_(color_enabled)
            {
                init_colors();
            }

            /**
             * @brief 构造函数（QPlainTextEdit 版本）
             */
            qt_textedit_sink(QPlainTextEdit* plain_text_edit,
                std::size_t max_lines = 1000,
                bool auto_scroll = true,
                bool color_enabled = true)
                : text_edit_(nullptr)
                , plain_text_edit_(plain_text_edit)
                , max_lines_(max_lines)
                , auto_scroll_(auto_scroll)
                , color_enabled_(color_enabled)
            {
                init_colors();
            }

            ~qt_textedit_sink() override = default;

            qt_textedit_sink(const qt_textedit_sink&) = delete;
            qt_textedit_sink& operator=(const qt_textedit_sink&) = delete;

        protected:
            void sink_it_(const details::log_msg& msg) override
            {
                memory_buf_t formatted;
                base_sink<Mutex>::formatter_->format(msg, formatted);
                std::string text = fmt::to_string(formatted);

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
                // Qt 5.10+ 支持 lambda 表达式
                if (text_edit_)
                {
                    QMetaObject::invokeMethod(text_edit_, [this, text, level = msg.level]() {
                        append_log(text_edit_, text, level);
                        }, Qt::QueuedConnection);
                }
                else if (plain_text_edit_)
                {
                    QMetaObject::invokeMethod(plain_text_edit_, [this, text, level = msg.level]() {
                        append_log(plain_text_edit_, text, level);
                        }, Qt::QueuedConnection);
                }
#else
                // Qt 5.9 及更早版本：使用旧的 invokeMethod 语法
                if (text_edit_)
                {
                    // 使用 HTML 格式输出（因为无法传递复杂参数）
                    QMetaObject::invokeMethod(
                        text_edit_,
                        "append",
                        Qt::QueuedConnection,
                        Q_ARG(QString, format_message(text, msg.level))
                    );

                    // 处理行数限制和自动滚动
                    QMetaObject::invokeMethod(
                        text_edit_,
                        "ensureCursorVisible",
                        Qt::QueuedConnection
                    );
                }
                else if (plain_text_edit_)
                {
                    QMetaObject::invokeMethod(
                        plain_text_edit_,
                        "appendPlainText",
                        Qt::QueuedConnection,
                        Q_ARG(QString, QString::fromStdString(text))
                    );

                    QMetaObject::invokeMethod(
                        plain_text_edit_,
                        "ensureCursorVisible",
                        Qt::QueuedConnection
                    );
                }
#endif
            }

            void flush_() override
            {
                // Qt 的 GUI 操作会自动刷新，这里不需要额外操作
            }

        private:
            void init_colors()
            {
                // 初始化颜色映射
                colors_[level::trace] = QColor(Qt::gray);
                colors_[level::debug] = QColor(Qt::black);
                colors_[level::info] = QColor(Qt::darkGreen);
                colors_[level::warn] = QColor(255, 140, 0);  // 深橙色
                colors_[level::err] = QColor(Qt::red);
                colors_[level::critical] = QColor(139, 0, 0);  // 深红色
            }

            QString format_message(const std::string& text, level::level_enum level)
            {
                if (!color_enabled_)
                {
                    return QString::fromStdString(text);
                }

                // 使用 HTML 格式来实现颜色（兼容性更好）
                QColor color = colors_[level];
                QString html = QString("<span style=\"color:%1;\">%2</span>")
                    .arg(color.name())
                    .arg(QString::fromStdString(text).toHtmlEscaped());

                return html;
            }

            void append_log(QTextEdit* edit, const std::string& text, level::level_enum level)
            {
                if (!edit) return;

                QTextCursor cursor(edit->document());
                cursor.movePosition(QTextCursor::End);

                if (color_enabled_)
                {
                    QTextCharFormat format;
                    format.setForeground(QBrush(colors_[level]));
                    cursor.setCharFormat(format);
                }

                cursor.insertText(QString::fromStdString(text));

                // 限制最大行数
                if (max_lines_ > 0)
                {
                    limit_document_size(edit);
                }

                // 自动滚动到底部
                if (auto_scroll_)
                {
                    edit->verticalScrollBar()->setValue(edit->verticalScrollBar()->maximum());
                }
            }

            void append_log(QPlainTextEdit* edit, const std::string& text, level::level_enum level)
            {
                if (!edit) return;

                QTextCursor cursor(edit->document());
                cursor.movePosition(QTextCursor::End);

                if (color_enabled_)
                {
                    QTextCharFormat format;
                    format.setForeground(QBrush(colors_[level]));
                    cursor.setCharFormat(format);
                }

                cursor.insertText(QString::fromStdString(text));

                // 限制最大行数
                if (max_lines_ > 0)
                {
                    limit_document_size(edit);
                }

                // 自动滚动到底部
                if (auto_scroll_)
                {
                    edit->verticalScrollBar()->setValue(edit->verticalScrollBar()->maximum());
                }
            }

            void limit_document_size(QTextEdit* edit)
            {
                QTextDocument* doc = edit->document();
                while (doc->blockCount() > static_cast<int>(max_lines_))
                {
                    QTextCursor cursor(doc);
                    cursor.movePosition(QTextCursor::Start);
                    cursor.select(QTextCursor::BlockUnderCursor);
                    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
            }

            void limit_document_size(QPlainTextEdit* edit)
            {
                QTextDocument* doc = edit->document();
                while (doc->blockCount() > static_cast<int>(max_lines_))
                {
                    QTextCursor cursor(doc);
                    cursor.movePosition(QTextCursor::Start);
                    cursor.select(QTextCursor::BlockUnderCursor);
                    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
            }

        private:
            QTextEdit* text_edit_;
            QPlainTextEdit* plain_text_edit_;
            std::size_t max_lines_;
            bool auto_scroll_;
            bool color_enabled_;
            std::map<level::level_enum, QColor> colors_;
        };

        // 类型别名
        using qt_textedit_sink_mt = qt_textedit_sink<std::mutex>;
        using qt_textedit_sink_st = qt_textedit_sink<details::null_mutex>;
    } // namespace sinks

    // 工厂函数
    template<typename Factory = spdlog::synchronous_factory>
    inline std::shared_ptr<logger> qt_textedit_logger_mt(
        const std::string& logger_name,
        QTextEdit* text_edit,
        std::size_t max_lines = 1000,
        bool auto_scroll = true,
        bool color_enabled = true)
    {
        return Factory::template create<sinks::qt_textedit_sink_mt>(
            logger_name, text_edit, max_lines, auto_scroll, color_enabled);
    }

    template<typename Factory = spdlog::synchronous_factory>
    inline std::shared_ptr<logger> qt_textedit_logger_mt(
        const std::string& logger_name,
        QPlainTextEdit* plain_text_edit,
        std::size_t max_lines = 1000,
        bool auto_scroll = true,
        bool color_enabled = true)
    {
        return Factory::template create<sinks::qt_textedit_sink_mt>(
            logger_name, plain_text_edit, max_lines, auto_scroll, color_enabled);
    }
} // namespace spdlog