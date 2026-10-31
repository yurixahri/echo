#ifndef MARKDOWN_HELPER_H
#define MARKDOWN_HELPER_H

#include <QString>
#include <QFont>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QRegularExpression>
#include "utils/theme.h"

inline QString highlight_code_syntax(QString code) {
    code = code.toHtmlEscaped();

    static const QRegularExpression token_re(
        "(//[^\r\n]*|/\\*[\\s\\S]*?\\*/|(?<=^|\\s)#[^\r\n]*)|"  // 1: comments
        "(&quot;[^&\\r\\n]*&quot;|&apos;[^&\\r\\n]*&apos;)|"    // 2: strings
        "(\\b(?:int|char|bool|void|float|double|auto|const|static|struct|class|enum|if|else|for|while|return|switch|case|break|continue|new|delete|nullptr|true|false|import|from|def|function|var|let|async|await|fn|pub|mut|impl|SELECT|FROM|WHERE|INSERT|INTO|UPDATE|DELETE)\\b)|" // 3: keywords
        "(\\b\\d+\\b)" // 4: numbers
    );

    QString result;
    int last_pos = 0;
    QRegularExpressionMatchIterator it = token_re.globalMatch(code);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        result.append(code.mid(last_pos, m.capturedStart() - last_pos));
        if (!m.captured(1).isEmpty()) {
            result.append(QString("<span style=\"color:%1; font-style:italic;\">%2</span>").arg(global_theme.muted_foreground, m.captured(1)));
        } else if (!m.captured(2).isEmpty()) {
            result.append(QString("<span style=\"color:%1;\">%2</span>").arg(global_theme.ring, m.captured(2)));
        } else if (!m.captured(3).isEmpty()) {
            result.append(QString("<span style=\"color:%1; font-weight:bold;\">%2</span>").arg(global_theme.primary, m.captured(3)));
        } else if (!m.captured(4).isEmpty()) {
            result.append(QString("<span style=\"color:%1;\">%2</span>").arg(global_theme.destructive, m.captured(4)));
        }
        last_pos = m.capturedEnd();
    }
    result.append(code.mid(last_pos));
    return result;
}

inline bool has_markdown_syntax(const QString &text) {
    if (text.trimmed().isEmpty()) return false;
    return text.contains("```") || text.contains('`') ||
           text.contains("**")  || text.contains('*') ||
           text.contains("<u>") || text.contains("</u>") ||
           text.contains("~~")  || text.contains('#') ||
           text.contains(">")   || text.contains('[') ||
           text.contains("- ");
}

inline QString sanitize_markdown_html(QString text) {
    static const QRegularExpression dangerous_tags(
        "<(script|style|iframe|object|embed)[^>]*>[\\s\\S]*?</\\1>",
        QRegularExpression::CaseInsensitiveOption);
    text.replace(dangerous_tags, "");

    static const QRegularExpression font_tag(
        "</?font[^>]*>",
        QRegularExpression::CaseInsensitiveOption);
    text.replace(font_tag, "");

    static const QRegularExpression font_size_style(
        "font-size\\s*:[^;\"'>]*",
        QRegularExpression::CaseInsensitiveOption);
    text.replace(font_size_style, "font-size:13px");

    static const QRegularExpression size_attr(
        "\\bsize\\s*=\\s*[\"']?[^\"'>\\s]+[\"']?",
        QRegularExpression::CaseInsensitiveOption);
    text.replace(size_attr, "");

    return text;
}

inline QString render_markdown_to_html_impl(const QString &markdown,
                                            const QFont *default_font) {
    if (markdown.trimmed().isEmpty()) return "";

    QString processed = markdown;

    // Auto-close open code block if odd count of triple backticks
    int count = 0;
    int pos = 0;
    while ((pos = processed.indexOf("```", pos)) != -1) {
        count++;
        pos += 3;
    }
    if (count % 2 != 0) {
        processed += "\n```";
    }

    // Extract all multi-line code blocks before passing to QTextDocument
    QList<QString> code_blocks;
    static const QRegularExpression code_block_re("```(?:[a-zA-Z0-9_+-]*\r?\n)?([\\s\\S]*?)```");
    QRegularExpressionMatchIterator it = code_block_re.globalMatch(processed);

    QString tokenized;
    int last_pos = 0;
    int block_idx = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        tokenized.append(sanitize_markdown_html(processed.mid(last_pos, m.capturedStart() - last_pos)));
        tokenized.append(QString("\n\n@@@CODEBLOCK_%1@@@\n\n").arg(block_idx));
        code_blocks.append(m.captured(1));
        last_pos = m.capturedEnd();
        block_idx++;
    }
    tokenized.append(sanitize_markdown_html(processed.mid(last_pos)));

    QTextDocument doc;
    doc.setDocumentMargin(0);
    QFont base_font;
    if (default_font) {
        base_font = *default_font;
    } else {
        base_font = QApplication::font();
        base_font.setPixelSize(13);
    }
    doc.setDefaultFont(base_font);
    doc.setMarkdown(tokenized, QTextDocument::MarkdownDialectGitHub);

    QString css = QString(
        "body { margin: 0; padding: 0; }"
        "h1 { font-size: 15px; font-weight: bold; margin: 4px 0 2px 0; color: %2; }"
        "h2 { font-size: 14px; font-weight: bold; margin: 3px 0 2px 0; }"
        "h3, h4, h5, h6 { font-size: 13px; font-weight: bold; margin: 2px 0 1px 0; }"
        "code { background-color: %1; color: %2; font-family: Consolas, monospace; font-size: 12px; border-radius: 3px; padding: 2px 4px; }"
        "a { color: %2; text-decoration: underline; font-weight: bold; }"
        "blockquote { border-left: 3px solid %2; margin: 0; padding-left: 8px; color: %3; }"
        "p { margin: 0; padding: 0; }"
    ).arg(global_theme.popover, global_theme.primary, global_theme.muted_foreground);

    doc.setDefaultStyleSheet(css);
    QString html = doc.toHtml();

    static const QRegularExpression large_font_re(
        "font-size\\s*:\\s*(?:[2-9]\\d|\\d{3,})(?:px|pt)",
        QRegularExpression::CaseInsensitiveOption);
    html.replace(large_font_re, "font-size: 14px");

    // Re-insert single unified code blocks with preserved newlines & spaces
    for (int i = 0; i < code_blocks.size(); ++i) {
        QString raw_code = code_blocks[i];
        while (raw_code.startsWith('\n') || raw_code.startsWith('\r')) raw_code.remove(0, 1);
        while (raw_code.endsWith('\n') || raw_code.endsWith('\r')) raw_code.chop(1);

        raw_code.replace("\t", "    ");

        QString highlighted = highlight_code_syntax(raw_code);
        highlighted.replace("  ", "&nbsp;&nbsp;");
        highlighted.replace("\r\n", "<br/>");
        highlighted.replace("\n", "<br/>");

        QString styled_pre = QString(
            "<div style=\"background-color:%1; border:1px solid %2; border-radius:5px; padding:6px 10px; margin:4px 0px; font-family:Consolas,'Courier New',monospace; font-size:12px; line-height:1.4; color:%3;\">"
            "<code>%4</code>"
            "</div>"
        ).arg(global_theme.popover, global_theme.border, global_theme.foreground, highlighted);

        QRegularExpression p_re(QString("<p[^>]*>\\s*@@@CODEBLOCK_%1@@@\\s*</p>").arg(i));
        if (html.contains(p_re)) {
            html.replace(p_re, styled_pre);
        } else {
            html.replace(QString("@@@CODEBLOCK_%1@@@").arg(i), styled_pre);
        }
    }

    return html;
}

inline QString render_markdown_to_html(const QString &markdown) {
    return render_markdown_to_html_impl(markdown, nullptr);
}

inline QString render_markdown_to_html(const QString &markdown,
                                       const QFont &default_font) {
    return render_markdown_to_html_impl(markdown, &default_font);
}

#endif // MARKDOWN_HELPER_H
