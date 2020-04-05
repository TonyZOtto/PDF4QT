//    Copyright (C) 2020 Jakub Melka
//
//    This file is part of PdfForQt.
//
//    PdfForQt is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    PdfForQt is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public License
//    along with PDFForQt.  If not, see <https://www.gnu.org/licenses/>.

#include "pdfexamplesgenerator.h"

#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"

#include <QCoreApplication>

void PDFExamplesGenerator::generateAnnotationsExample()
{
    pdf::PDFDocumentBuilder builder;
    builder.setDocumentTitle("Test document");
    builder.setDocumentAuthor("Jakub Melka");
    builder.setDocumentCreator(QCoreApplication::applicationName());
    builder.setDocumentSubject("Testing annotations");
    builder.setLanguage(QLocale::system());

    pdf::PDFObjectReference page1 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationText(page1, QRectF(50,  50, 24, 24), pdf::TextAnnotationIcon::Comment, "Title1", "Subject1", "Comment", false);
    builder.createAnnotationText(page1, QRectF(50, 100, 24, 24), pdf::TextAnnotationIcon::Help, "Title1", "Subject1", "Help", false);
    builder.createAnnotationText(page1, QRectF(50, 150, 24, 24), pdf::TextAnnotationIcon::Insert, "Title1", "Subject1", "Insert", false);
    builder.createAnnotationText(page1, QRectF(50, 200, 24, 24), pdf::TextAnnotationIcon::Key, "Title1", "Subject1", "Key", false);
    builder.createAnnotationText(page1, QRectF(50, 250, 24, 24), pdf::TextAnnotationIcon::NewParagraph, "Title1", "Subject1", "NewParagraph", false);
    builder.createAnnotationText(page1, QRectF(50, 300, 24, 24), pdf::TextAnnotationIcon::Note, "Title1", "Subject1", "Note", false);
    builder.createAnnotationText(page1, QRectF(50, 350, 24, 24), pdf::TextAnnotationIcon::Paragraph, "Title1", "Subject1", "Paragraph", false);
    builder.createAnnotationText(page1, QRectF(250,  50, 24, 24), pdf::TextAnnotationIcon::Comment, "Title1", "Subject1", "Comment", true);
    builder.createAnnotationText(page1, QRectF(250, 100, 24, 24), pdf::TextAnnotationIcon::Help, "Title1", "Subject1", "Help", true);
    builder.createAnnotationText(page1, QRectF(250, 150, 24, 24), pdf::TextAnnotationIcon::Insert, "Title1", "Subject1", "Insert", true);
    builder.createAnnotationText(page1, QRectF(250, 200, 24, 24), pdf::TextAnnotationIcon::Key, "Title1", "Subject1", "Key", true);
    builder.createAnnotationText(page1, QRectF(250, 250, 24, 24), pdf::TextAnnotationIcon::NewParagraph, "Title1", "Subject1", "NewParagraph", true);
    builder.createAnnotationText(page1, QRectF(250, 300, 24, 24), pdf::TextAnnotationIcon::Note, "Title1", "Subject1", "Note", true);
    builder.createAnnotationText(page1, QRectF(250, 350, 24, 24), pdf::TextAnnotationIcon::Paragraph, "Title1", "Subject1", "Paragraph", true);

    pdf::PDFObjectReference page2 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationLink(page2, QRectF(50,  50, 200, 50), "www.seznam.cz", pdf::LinkHighlightMode::Invert);
    builder.createAnnotationLink(page2, QRectF(50,  150, 200, 50), "www.seznam.cz", pdf::LinkHighlightMode::None);
    builder.createAnnotationLink(page2, QRectF(50,  250, 200, 50), "www.seznam.cz", pdf::LinkHighlightMode::Outline);
    builder.createAnnotationLink(page2, QRectF(50,  350, 200, 50), "www.seznam.cz", pdf::LinkHighlightMode::Push);

    pdf::PDFObjectReference page3 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationFreeText(page3, QRectF(50,  50, 100, 50), "Title", "Subject", "Toto je dolni text, желтая лошадь", Qt::AlignLeft);
    builder.createAnnotationFreeText(page3, QRectF(50,  150, 100, 50), "Title", "Subject", "Toto je stredni text, желтая лошадь", Qt::AlignCenter);
    builder.createAnnotationFreeText(page3, QRectF(50,  250, 100, 50), "Title", "Subject", "Toto je horni text, желтая лошадь", Qt::AlignRight);
    builder.createAnnotationFreeText(page3, QRectF(250,  50, 100, 50), QRectF(300, 50, 50, 50), "Title", "Subject", "Toto je dolni text, желтая лошадь", Qt::AlignLeft, QPointF(250, 50), QPointF(300, 100), pdf::AnnotationLineEnding::OpenArrow, pdf::AnnotationLineEnding::ClosedArrow);
    builder.createAnnotationFreeText(page3, QRectF(250,  150, 100, 50), QRectF(300, 150, 50, 50), "Title", "Subject", "Toto je stredni text, желтая лошадь", Qt::AlignCenter, QPointF(250, 150), QPointF(300, 200), pdf::AnnotationLineEnding::OpenArrow, pdf::AnnotationLineEnding::ClosedArrow);
    pdf::PDFObjectReference ref = builder.createAnnotationFreeText(page3, QRectF(250,  250, 100, 50), QRectF(300, 250, 50, 50), "Title", "Subject", "Toto je horni text, желтая лошадь", Qt::AlignRight, QPointF(260, 250), QPointF(260, 290), QPointF(300, 290), pdf::AnnotationLineEnding::OpenArrow, pdf::AnnotationLineEnding::ClosedArrow);
    builder.setAnnotationContents(ref, "UPDATED: Horni text");
    builder.setAnnotationTitle(ref, "Updated title");
    builder.setAnnotationSubject(ref, "Updated subject");
    builder.setAnnotationOpacity(ref, 0.5);

    {
        pdf::PDFObjectReference page4 = builder.appendPage(QRectF(0, 0, 400, 400));
        QRectF baseRect = QRectF(0, 0, 100, 50);
        qreal spaceCoef = 1.2;
        int lineRows = 400 / (baseRect.height() * spaceCoef);
        int lineCols = 400 / (baseRect.width() * spaceCoef);
        int lineNumber = 0;
        constexpr pdf::AnnotationLineEnding lineEndings[] =
        {
            pdf::AnnotationLineEnding::None,
            pdf::AnnotationLineEnding::Square,
            pdf::AnnotationLineEnding::Circle,
            pdf::AnnotationLineEnding::Diamond,
            pdf::AnnotationLineEnding::OpenArrow,
            pdf::AnnotationLineEnding::ClosedArrow,
            pdf::AnnotationLineEnding::Butt,
            pdf::AnnotationLineEnding::ROpenArrow,
            pdf::AnnotationLineEnding::RClosedArrow,
            pdf::AnnotationLineEnding::Slash
        };
        for (int i = 0; i < lineCols; ++i)
        {
            for (int j = 0; j < lineRows; ++j)
            {
                QRectF rect = baseRect.translated(i * baseRect.width() * spaceCoef, j * baseRect.height() * spaceCoef);

                QPointF start;
                QPointF end;
                if (lineNumber % 2 == 0)
                {
                    start = rect.topLeft();
                    end = rect.bottomRight();
                }
                else
                {
                    start = rect.bottomLeft();
                    end = rect.topRight();
                }

                pdf::AnnotationLineEnding lineEnding = lineEndings[lineNumber % std::size(lineEndings)];
                builder.createAnnotationLine(page4, rect, start, end, 2.0, Qt::yellow, Qt::green, "Title", "Subject", "Contents", lineEnding, lineEnding);
                ++lineNumber;
            }
        }
    }

    {
        pdf::PDFObjectReference page4 = builder.appendPage(QRectF(0, 0, 400, 400));
        QRectF baseRect = QRectF(0, 0, 100, 50);
        qreal spaceCoef = 1.2;
        int lineRows = 400 / (baseRect.height() * spaceCoef);
        int lineCols = 400 / (baseRect.width() * spaceCoef);
        int lineNumber = 0;
        constexpr pdf::AnnotationLineEnding lineEndings[] =
        {
            pdf::AnnotationLineEnding::None,
            pdf::AnnotationLineEnding::Square,
            pdf::AnnotationLineEnding::Circle,
            pdf::AnnotationLineEnding::Diamond,
            pdf::AnnotationLineEnding::OpenArrow,
            pdf::AnnotationLineEnding::ClosedArrow,
            pdf::AnnotationLineEnding::Butt,
            pdf::AnnotationLineEnding::ROpenArrow,
            pdf::AnnotationLineEnding::RClosedArrow,
            pdf::AnnotationLineEnding::Slash
        };
        for (int i = 0; i < lineCols; ++i)
        {
            for (int j = 0; j < lineRows; ++j)
            {
                QRectF rect = baseRect.translated(i * baseRect.width() * spaceCoef, j * baseRect.height() * spaceCoef);

                QPointF start(rect.left(), rect.center().y());
                QPointF end(rect.right(), rect.center().y());

                pdf::AnnotationLineEnding lineEnding = lineEndings[lineNumber % std::size(lineEndings)];
                builder.createAnnotationLine(page4, rect, start, end, 2.0, Qt::yellow, Qt::green, "Title", "Subject", "Contents",
                                             lineEnding, lineEnding, 10.0, 2.0, 3.0, true, (j % 2 == 0));
                ++lineNumber;
            }
        }
    }

    pdf::PDFObjectReference page5 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationSquare(page5, QRectF(50, 50, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
    builder.createAnnotationSquare(page5, QRectF(50, 150, 50, 50), 3.0, QColor(), Qt::red, "Title2", "Subject2", "Contents - red boundary");
    builder.createAnnotationSquare(page5, QRectF(50, 250, 50, 50), 3.0, Qt::green, QColor(), "Title3", "Subject3", "Contents - green filling");

    {
        pdf::PDFObjectReference page5 = builder.appendPage(QRectF(0, 0, 400, 400));

        {
            pdf::PDFObjectReference annotation = builder.createAnnotationSquare(page5, QRectF(50, 50, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
            builder.setAnnotationBorderStyle(annotation, pdf::PDFAnnotationBorder::Style::Solid, 2.718);
            builder.setAnnotationColor(annotation, Qt::black);
            builder.updateAnnotationAppearanceStreams(annotation);
        }

        {
            pdf::PDFObjectReference annotation = builder.createAnnotationSquare(page5, QRectF(50, 150, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
            builder.setAnnotationBorderStyle(annotation, pdf::PDFAnnotationBorder::Style::Underline, 2.718);
            builder.setAnnotationColor(annotation, Qt::black);
            builder.updateAnnotationAppearanceStreams(annotation);
        }

        {
            pdf::PDFObjectReference annotation = builder.createAnnotationSquare(page5, QRectF(50, 250, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
            builder.setAnnotationBorderStyle(annotation, pdf::PDFAnnotationBorder::Style::Inset, 2.718);
            builder.setAnnotationColor(annotation, Qt::black);
            builder.updateAnnotationAppearanceStreams(annotation);
        }

        {
            pdf::PDFObjectReference annotation = builder.createAnnotationSquare(page5, QRectF(150, 50, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
            builder.setAnnotationBorderStyle(annotation, pdf::PDFAnnotationBorder::Style::Beveled, 2.718);
            builder.setAnnotationColor(annotation, Qt::black);
            builder.updateAnnotationAppearanceStreams(annotation);
        }

        {
            pdf::PDFObjectReference annotation = builder.createAnnotationSquare(page5, QRectF(150, 150, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
            builder.setAnnotationBorderStyle(annotation, pdf::PDFAnnotationBorder::Style::Dashed, 2.718);
            builder.setAnnotationColor(annotation, Qt::black);
            builder.updateAnnotationAppearanceStreams(annotation);
        }

        {
            pdf::PDFObjectReference annotation = builder.createAnnotationSquare(page5, QRectF(150, 250, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
            builder.setAnnotationBorder(annotation, 5.0, 3.0, 2.0);
            builder.setAnnotationColor(annotation, Qt::black);
            builder.updateAnnotationAppearanceStreams(annotation);
        }
    }

    pdf::PDFObjectReference page6 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationCircle(page6, QRectF(50, 50, 50, 50), 3.0, Qt::green, Qt::red, "Title1", "Subject1", "Contents - green filling, red boundary");
    builder.createAnnotationCircle(page6, QRectF(50, 150, 50, 50), 3.0, QColor(), Qt::red, "Title2", "Subject2", "Contents - red boundary");
    builder.createAnnotationCircle(page6, QRectF(50, 250, 50, 50), 3.0, Qt::green, QColor(), "Title3", "Subject3", "Contents - green filling");

    pdf::PDFObjectReference page7 = builder.appendPage(QRectF(0, 0, 400, 400));
    QPolygonF polygon;
    QRectF polygonRect(50, 50, 50, 50);
    polygon << polygonRect.topLeft() << polygonRect.center() << polygonRect.topRight() << polygonRect.bottomRight() << polygonRect.bottomLeft() << polygonRect.topLeft();
    builder.createAnnotationPolygon(page7, polygon, 2.0, Qt::green, Qt::yellow, "Title", "Subject", "Contents");
    polygon.translate(0, 100);
    builder.createAnnotationPolygon(page7, polygon, 2.0, Qt::red, Qt::cyan, "Title", "Subject", "Contents");
    polygon.pop_back();
    polygon.translate(200, 0);
    builder.createAnnotationPolyline(page7, polygon, 2.0, Qt::green, Qt::yellow, "Title", "Subject", "Contents", pdf::AnnotationLineEnding::ClosedArrow, pdf::AnnotationLineEnding::Circle);
    polygon.translate(0, -100);
    builder.createAnnotationPolyline(page7, polygon, 2.0, Qt::green, Qt::yellow, "Title", "Subject", "Contents", pdf::AnnotationLineEnding::ClosedArrow, pdf::AnnotationLineEnding::Circle);

    pdf::PDFObjectReference page8 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationHighlight(page8, QRectF(50, 50, 50, 50), Qt::yellow, "Title1", "Subject1", "Contents - green filling, red boundary");
    builder.createAnnotationHighlight(page8, QRectF(50, 150, 50, 50), Qt::green);
    builder.createAnnotationHighlight(page8, QRectF(50, 250, 50, 50), Qt::red);

    pdf::PDFObjectReference page9 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationUnderline(page9, QRectF(50, 50, 50, 50), Qt::yellow, "Title1", "Subject1", "Contents - green filling, red boundary");
    builder.createAnnotationUnderline(page9, QRectF(50, 150, 50, 50), Qt::green);
    builder.createAnnotationUnderline(page9, QRectF(50, 250, 50, 50), Qt::red);

    pdf::PDFObjectReference page10 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationSquiggly(page10, QRectF(50, 50, 50, 50), Qt::yellow, "Title1", "Subject1", "Contents - green filling, red boundary");
    builder.createAnnotationSquiggly(page10, QRectF(50, 150, 50, 50), Qt::green);
    builder.createAnnotationSquiggly(page10, QRectF(50, 250, 50, 50), Qt::red);

    pdf::PDFObjectReference page11 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationStrikeout(page11, QRectF(50, 50, 50, 50), Qt::yellow, "Title1", "Subject1", "Contents - green filling, red boundary");
    builder.createAnnotationStrikeout(page11, QRectF(50, 150, 50, 50), Qt::green);
    builder.createAnnotationStrikeout(page11, QRectF(50, 250, 50, 50), Qt::red);

    pdf::PDFObjectReference page12 = builder.appendPage(QRectF(0, 0, 400, 400));
    builder.createAnnotationCaret(page12, QRectF(50, 50, 50, 50), 3.0, Qt::blue, "Title1", "Subject1", "Contents - green filling, red boundary");
    builder.createAnnotationCaret(page12, QRectF(50, 150, 50, 50), 3.0, Qt::red, "Title2", "Subject2", "Contents - red boundary");
    builder.createAnnotationCaret(page12, QRectF(50, 250, 50, 50), 3.0, Qt::green, "Title3", "Subject3", "Contents - green filling");

    pdf::PDFObjectReference page13 = builder.appendPage(QRectF(0, 0, 400, 400));
    {
        QPolygonF polygon;
        polygon << QPointF(50, 50);
        polygon << QPointF(50, 100);
        polygon << QPointF(100, 50);
        builder.createAnnotationInk(page13, polygon, 2.0, Qt::red, "Title", "Subject", "Contents");
    }
    {
        QPolygonF polygon;
        polygon << QPointF(50, 50);
        polygon << QPointF(50, 100);
        polygon << QPointF(100, 50);
        polygon.translate(150, 0);
        builder.createAnnotationInk(page13, polygon, 2.0, Qt::red, "Title", "Subject", "Contents");
    }

    {
        pdf::Polygons polygons;
        QPolygonF polygon;
        polygon << QPointF(50, 50);
        polygon << QPointF(50, 100);
        polygon << QPointF(100, 50);
        polygon << QPointF(50, 50);
        polygon.translate(0, 150);
        polygons.push_back(polygon);
        polygon.translate(150, 0);
        polygons.push_back(polygon);
        builder.createAnnotationInk(page13, polygons, 2.0, Qt::red, "Title", "Subject", "Contents");
    }

    // Write result to a file
    pdf::PDFDocument document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    writer.write("Ex_Annotations.pdf", &document);
}
