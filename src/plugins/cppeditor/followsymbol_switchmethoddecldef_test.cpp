/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "cppeditor.h"
#include "cppeditorplugin.h"
#include "cppeditortestcase.h"
#include "cppelementevaluator.h"
#include "cppvirtualfunctionassistprovider.h"
#include "cppvirtualfunctionproposalitem.h"

#include <texteditor/codeassist/basicproposalitemlistmodel.h>
#include <texteditor/codeassist/iassistprocessor.h>
#include <texteditor/codeassist/iassistproposal.h>

#include <utils/fileutils.h>

#include <QDebug>
#include <QDir>
#include <QtTest>

/*!
    Tests for Follow Symbol Under Cursor and Switch Between Function Declaration/Definition

    Section numbers refer to

        Working Draft, Standard for Programming Language C++
        Document Number: N3242=11-0012

    You can find potential test code for Follow Symbol Under Cursor on the bottom of this file.
 */

using namespace CPlusPlus;
using namespace CppEditor;
using namespace CppEditor::Internal;
using namespace CppTools;
using namespace TextEditor;
using namespace Core;

class OverrideItem {
public:
    OverrideItem() : line(0) {}
    OverrideItem(const QString &text, int line = 0) : text(text), line(line) {}
    bool isValid() { return line != 0; }

    QString text;
    int line;
};
typedef QList<OverrideItem> OverrideItemList;
Q_DECLARE_METATYPE(OverrideItem)
Q_DECLARE_METATYPE(OverrideItemList)

inline bool operator==(const OverrideItem &lhs, const OverrideItem &rhs)
{
    return lhs.text == rhs.text && lhs.line == rhs.line;
}

QT_BEGIN_NAMESPACE
namespace QTest {
template<> char *toString(const OverrideItem &data)
{
    QByteArray ba = "OverrideItem(";
    ba += data.text.toLatin1() + ", " + QByteArray::number(data.line);
    ba += ")";
    return qstrdup(ba.data());
}
}
QT_END_NAMESPACE

namespace {

typedef QByteArray _;

/// A fake virtual functions assist provider that runs processor->perform() already in configure()
class VirtualFunctionTestAssistProvider : public VirtualFunctionAssistProvider
{
public:
    VirtualFunctionTestAssistProvider(CPPEditorWidget *editorWidget)
        : m_editorWidget(editorWidget)
    {}

    // Invoke the processor already here to calculate the proposals. Return false in order to
    // indicate that configure failed, so the actual code assist invocation leading to a pop-up
    // will not happen.
    bool configure(const VirtualFunctionAssistProvider::Parameters &params)
    {
        VirtualFunctionAssistProvider::configure(params);

        IAssistProcessor *processor = createProcessor();
        IAssistInterface *assistInterface
                = m_editorWidget->createAssistInterface(FollowSymbol, ExplicitlyInvoked);
        IAssistProposal *immediateProposal = processor->immediateProposal(assistInterface);
        IAssistProposal *finalProposal = processor->perform(assistInterface);

        VirtualFunctionAssistProvider::clearParams();

        m_immediateItems = itemList(immediateProposal->model());
        m_finalItems = itemList(finalProposal->model());

        return false;
    }

    static OverrideItemList itemList(IAssistProposalModel *imodel)
    {
        OverrideItemList result;
        BasicProposalItemListModel *model = dynamic_cast<BasicProposalItemListModel *>(imodel);
        if (!model)
            return result;

        // Mimic relevant GenericProposalWidget::showProposal() calls
        model->removeDuplicates();
        model->reset();
        if (model->isSortable(QString()))
            model->sort(QString());

        for (int i = 0, size = model->size(); i < size; ++i) {
            VirtualFunctionProposalItem *item
                = dynamic_cast<VirtualFunctionProposalItem *>(model->proposalItem(i));

            const QString text = model->text(i);
            const int line = item->link().targetLine;
//            Uncomment for updating/generating reference data:
//            qDebug("<< OverrideItem(QLatin1String(\"%s\"), %d)", qPrintable(text), line);
            result << OverrideItem(text, line);
        }

        return result;
    }

public:
    OverrideItemList m_immediateItems;
    OverrideItemList m_finalItems;

private:
    CPPEditorWidget *m_editorWidget;
};

class TestDocument;
typedef QSharedPointer<TestDocument> TestDocumentPtr;

/**
 * Represents a test document.
 *
 * A TestDocument's source can contain special characters:
 *   - a '@' character denotes the initial text cursor position
 *   - a '$' character denotes the target text cursor position
 */
class TestDocument : public CppEditor::Internal::Tests::TestDocument
{
public:
    TestDocument(const QByteArray &source, const QByteArray &fileName)
        : CppEditor::Internal::Tests::TestDocument(fileName, source)
        , m_targetCursorPosition(source.indexOf('$'))
    {
        if (m_cursorPosition != -1 || m_targetCursorPosition != -1)
            QVERIFY(m_cursorPosition != m_targetCursorPosition);

        if (m_cursorPosition > m_targetCursorPosition) {
            m_source.remove(m_cursorPosition, 1);
            if (m_targetCursorPosition != -1) {
                m_source.remove(m_targetCursorPosition, 1);
                --m_cursorPosition;
            }
        } else {
            m_source.remove(m_targetCursorPosition, 1);
            if (m_cursorPosition != -1) {
                m_source.remove(m_cursorPosition, 1);
                --m_targetCursorPosition;
            }
        }
    }

    static TestDocumentPtr create(const QByteArray &source, const QByteArray &fileName)
    {
        return TestDocumentPtr(new TestDocument(source, fileName));
    }

    bool hasTargetCursorMarker() const { return m_targetCursorPosition != -1; }

public:
    int m_targetCursorPosition;
};

QList<TestDocumentPtr> singleDocument(const QByteArray &source)
{
    return QList<TestDocumentPtr>() << TestDocument::create(source, "file.cpp");
}

/**
 * Encapsulates the whole process of setting up several editors, positioning the cursor,
 * executing Follow Symbol Under Cursor or Switch Between Function Declaration/Definition
 * and checking the result.
 */
class F2TestCase : public CppEditor::Internal::Tests::TestCase
{
public:
    enum CppEditorAction {
        FollowSymbolUnderCursorAction,
        SwitchBetweenMethodDeclarationDefinitionAction
    };

    F2TestCase(CppEditorAction action,
               const QList<TestDocumentPtr> testFiles,
               const OverrideItemList &expectedVirtualFunctionProposal = OverrideItemList());

private:
    static TestDocumentPtr testFileWithInitialCursorMarker(const QList<TestDocumentPtr> &testFiles);
    static TestDocumentPtr testFileWithTargetCursorMarker(const QList<TestDocumentPtr> &testFiles);
};

/// Creates a test case with multiple test files.
/// Exactly one test document must be provided that contains '@', the initial position marker.
/// Exactly one test document must be provided that contains '$', the target position marker.
/// It can be the same document.
F2TestCase::F2TestCase(CppEditorAction action,
                       const QList<TestDocumentPtr> testFiles,
                       const OverrideItemList &expectedVirtualFunctionProposal)
{
    QVERIFY(succeededSoFar());

    // Check if there are initial and target position markers
    TestDocumentPtr initialTestFile = testFileWithInitialCursorMarker(testFiles);
    QVERIFY2(initialTestFile,
        "No test file with initial cursor marker is provided.");
    TestDocumentPtr targetTestFile = testFileWithTargetCursorMarker(testFiles);
    QVERIFY2(targetTestFile,
        "No test file with target cursor marker is provided.");

    // Write files to disk
    foreach (TestDocumentPtr testFile, testFiles)
        QVERIFY(testFile->writeToDisk());

    // Update Code Model
    QStringList filePaths;
    foreach (const TestDocumentPtr &testFile, testFiles)
        filePaths << testFile->filePath();
    QVERIFY(parseFiles(filePaths));

    // Open Files
    foreach (TestDocumentPtr testFile, testFiles) {
        QVERIFY(openCppEditor(testFile->filePath(), &testFile->m_editor,
                              &testFile->m_editorWidget));
        closeEditorAtEndOfTestCase(testFile->m_editor);

        // Wait until the indexer processed the just opened file.
        // The file is "Full Checked" since it is in the working copy now,
        // that is the function bodies are processed.
        forever {
            const Document::Ptr document = waitForFileInGlobalSnapshot(testFile->filePath());
            if (document->checkMode() == Document::FullCheck)
                break;
        }

        // Rehighlight
        waitForRehighlightedSemanticDocument(testFile->m_editorWidget);
    }

    // Activate editor of initial test file
    EditorManager::activateEditor(initialTestFile->m_editor);

    initialTestFile->m_editor->setCursorPosition(initialTestFile->m_cursorPosition);
//    qDebug() << "Initial line:" << initialTestFile->editor->currentLine();
//    qDebug() << "Initial column:" << initialTestFile->editor->currentColumn() - 1;

    OverrideItemList immediateVirtualSymbolResults;
    OverrideItemList finalVirtualSymbolResults;

    // Trigger the action
    switch (action) {
    case FollowSymbolUnderCursorAction: {
        CPPEditorWidget *widget = initialTestFile->m_editorWidget;
        FollowSymbolUnderCursor *delegate = widget->followSymbolUnderCursorDelegate();
        VirtualFunctionAssistProvider *original = delegate->virtualFunctionAssistProvider();

        // Set test provider, run and get results
        QScopedPointer<VirtualFunctionTestAssistProvider> testProvider(
            new VirtualFunctionTestAssistProvider(widget));
        delegate->setVirtualFunctionAssistProvider(testProvider.data());
        initialTestFile->m_editorWidget->openLinkUnderCursor();
        immediateVirtualSymbolResults = testProvider->m_immediateItems;
        finalVirtualSymbolResults = testProvider->m_finalItems;

        // Restore original test provider
        delegate->setVirtualFunctionAssistProvider(original);
        break;
    }
    case SwitchBetweenMethodDeclarationDefinitionAction:
        CppEditorPlugin::instance()->switchDeclarationDefinition();
        break;
    default:
        QFAIL("Unknown test action");
        break;
    }

    QCoreApplication::processEvents();

    // Compare
    IEditor *currentEditor = EditorManager::currentEditor();
    BaseTextEditor *currentTextEditor = dynamic_cast<BaseTextEditor*>(currentEditor);
    QVERIFY(currentTextEditor);

    QCOMPARE(currentTextEditor->document()->filePath(), targetTestFile->filePath());
    int expectedLine, expectedColumn;
    currentTextEditor->convertPosition(targetTestFile->m_targetCursorPosition,
                                       &expectedLine, &expectedColumn);
//    qDebug() << "Expected line:" << expectedLine;
//    qDebug() << "Expected column:" << expectedColumn;

    QEXPECT_FAIL("globalVarFromEnum", "Contributor works on a fix.", Abort);
    QCOMPARE(currentTextEditor->currentLine(), expectedLine);
    QCOMPARE(currentTextEditor->currentColumn() - 1, expectedColumn);

//    qDebug() << immediateVirtualSymbolResults;
//    qDebug() << finalVirtualSymbolResults;
    OverrideItemList expectedImmediate;
    if (!expectedVirtualFunctionProposal.isEmpty()) {
        expectedImmediate << expectedVirtualFunctionProposal.first();
        expectedImmediate << OverrideItem(QLatin1String("...searching overrides"));
    }
    QCOMPARE(immediateVirtualSymbolResults, expectedImmediate);
    QCOMPARE(finalVirtualSymbolResults, expectedVirtualFunctionProposal);
}

TestDocumentPtr F2TestCase::testFileWithInitialCursorMarker(const QList<TestDocumentPtr> &testFiles)
{
    foreach (const TestDocumentPtr testFile, testFiles) {
        if (testFile->hasCursorMarker())
            return testFile;
    }
    return TestDocumentPtr();
}

TestDocumentPtr F2TestCase::testFileWithTargetCursorMarker(const QList<TestDocumentPtr> &testFiles)
{
    foreach (const TestDocumentPtr testFile, testFiles) {
        if (testFile->hasTargetCursorMarker())
            return testFile;
    }
    return TestDocumentPtr();
}

} // anonymous namespace

Q_DECLARE_METATYPE(QList<TestDocumentPtr>)

void CppEditorPlugin::test_SwitchMethodDeclarationDefinition_data()
{
    QTest::addColumn<QByteArray>("header");
    QTest::addColumn<QByteArray>("source");

    QTest::newRow("fromFunctionDeclarationSymbol") << _(
        "class C\n"
        "{\n"
        "public:\n"
        "    C();\n"
        "    int @function();\n"  // Line 5
        "};\n"
        ) << _(
        "#include \"file.h\"\n"
        "\n"
        "C::C()\n"
        "{\n"
        "}\n"                   // Line 5
        "\n"
        "int C::$function()\n"
        "{\n"
        "    return 1 + 1;\n"
        "}\n"                   // Line 10
    );

    QTest::newRow("fromFunctionDefinitionSymbol") << _(
        "class C\n"
        "{\n"
        "public:\n"
        "    C();\n"
        "    int $function();\n"
        "};\n"
        ) << _(
        "#include \"file.h\"\n"
        "\n"
        "C::C()\n"
        "{\n"
        "}\n"
        "\n"
        "int C::@function()\n"
        "{\n"
        "    return 1 + 1;\n"
        "}\n"
    );

    QTest::newRow("fromFunctionBody") << _(
        "class C\n"
        "{\n"
        "public:\n"
        "    C();\n"
        "    int $function();\n"
        "};\n"
        ) << _(
        "#include \"file.h\"\n"
        "\n"
        "C::C()\n"
        "{\n"
        "}\n"                   // Line 5
        "\n"
        "int C::function()\n"
        "{\n"
        "    return @1 + 1;\n"
        "}\n"                   // Line 10
    );

    QTest::newRow("fromReturnType") << _(
        "class C\n"
        "{\n"
        "public:\n"
        "    C();\n"
        "    int $function();\n"
        "};\n"
        ) << _(
        "#include \"file.h\"\n"
        "\n"
        "C::C()\n"
        "{\n"
        "}\n"                   // Line 5
        "\n"
        "@int C::function()\n"
        "{\n"
        "    return 1 + 1;\n"
        "}\n"                   // Line 10
    );
}

void CppEditorPlugin::test_SwitchMethodDeclarationDefinition()
{
    QFETCH(QByteArray, header);
    QFETCH(QByteArray, source);

    const QList<TestDocumentPtr> testFiles = QList<TestDocumentPtr>()
        << TestDocument::create(header, "file.h")
        << TestDocument::create(source, "file.cpp");

    F2TestCase(F2TestCase::SwitchBetweenMethodDeclarationDefinitionAction, testFiles);
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_data()
{
    QTest::addColumn<QByteArray>("source");

    /// Check ...
    QTest::newRow("globalVarFromFunction") << _(
        "int $j;\n"
        "int main()\n"
        "{\n"
        "    @j = 2;\n"
        "}\n"           // Line 5
    );

    // 3.3.10 Name hiding (par 3.), from text
    QTest::newRow("funLocalVarHidesClassMember") << _(
        "struct C {\n"
        "    void f()\n"
        "    {\n"
        "        int $member; // hides C::member\n"
        "        ++@member;\n"                       // Line 5
        "    }\n"
        "    int member;\n"
        "};\n"
    );

    // 3.3.10 Name hiding (par 4.), from text
    QTest::newRow("funLocalVarHidesNamespaceMemberIntroducedByUsingDirective") << _(
        "namespace N {\n"
        "    int i;\n"
        "}\n"
        "\n"
        "int main()\n"                       // Line 5
        "{\n"
        "    using namespace N;\n"
        "    int $i;\n"
        "    ++i@; // refers to local i;\n"
        "}\n"                                // Line 10
    );

    // 3.3.3 Block scope (par. 4), from text
    // Same for if, while, switch
    QTest::newRow("loopLocalVarHidesOuterScopeVariable1") << _(
        "int main()\n"
        "{\n"
        "    int i = 1;\n"
        "    for (int $i = 0; i < 10; ++i) { // 'i' refers to for's i\n"
        "        i = @i; // same\n"                                       // Line 5
        "    }\n"
        "}\n"
    );

    // 3.3.3 Block scope (par. 4), from text
    // Same for if, while, switch
    QTest::newRow("loopLocalVarHidesOuterScopeVariable2") << _(
        "int main()\n"
        "{\n"
        "    int i = 1;\n"
        "    for (int $i = 0; @i < 10; ++i) { // 'i' refers to for's i\n"
        "        i = i; // same\n"                                         // Line 5
        "    }\n"
        "}\n"
    );

    // 3.3.7 Class scope, part of the example
    QTest::newRow("subsequentDefinedClassMember") << _(
        "class X {\n"
        "    int f() { return @i; } // i refers to class's i\n"
        "    int $i;\n"
        "};\n"
    );

    // 3.3.7 Class scope, part of the example
    // Variable name hides type name.
    QTest::newRow("classMemberHidesOuterTypeDef") << _(
        "typedef int c;\n"
        "class X {\n"
        "    int f() { return @c; } // c refers to class' c\n"
        "    int $c; // hides typedef name\n"
        "};\n"                                                 // Line 5
    );

    // 3.3.2 Point of declaration (par. 1), copy-paste
    QTest::newRow("globalVarFromEnum") << _(
        "const int $x = 12;\n"
        "int main()\n"
        "{\n"
        "    enum { x = @x }; // x refers to global x\n"
        "}\n"                                             // Line 5
    );

    // 3.3.2 Point of declaration
    QTest::newRow("selfInitialization") << _(
        "int x = 12;\n"
        "int main()\n"
        "{\n"
        "   int $x = @x; // Second x refers to local x\n"
        "}\n"                                              // Line 5
    );

    // 3.3.2 Point of declaration (par. 3), from text
    QTest::newRow("pointerToClassInClassDefinition") << _(
        "class $Foo {\n"
        "    @Foo *p; // Refers to above Foo\n"
        "};\n"
    );

    // 3.3.2 Point of declaration (par. 5), copy-paste
    QTest::newRow("previouslyDefinedMemberFromArrayDefinition") << _(
        "struct X {\n"
        "    enum E { $z = 16 };\n"
        "    int b[X::@z]; // z refers to defined z\n"
        "};\n"
    );

    // 3.3.7 Class scope (par. 2), from text
    QTest::newRow("outerStaticMemberVariableFromInsideSubclass") << _(
        "struct C\n"
        "{\n"
        "   struct I\n"
        "   {\n"
        "       void f()\n"                            // Line 5
        "       {\n"
        "           int i = @c; // refers to C's c\n"
        "       }\n"
        "   };\n"
        "\n"                                           // Line 10
        "   static int $c;\n"
        "};\n"
    );

    // 3.3.7 Class scope (par. 1), part of point 5
    QTest::newRow("memberVariableFollowingDotOperator") << _(
        "struct C\n"
        "{\n"
        "    int $member;\n"
        "};\n"
        "\n"                 // Line 5
        "int main()\n"
        "{\n"
        "    C c;\n"
        "    c.@member++;\n"
        "}\n"                // Line 10
    );

    // 3.3.7 Class scope (par. 1), part of point 5
    QTest::newRow("memberVariableFollowingArrowOperator") << _(
        "struct C\n"
        "{\n"
        "    int $member;\n"
        "};\n"
        "\n"                    // Line 5
        "int main()\n"
        "{\n"
        "    C* c;\n"
        "    c->@member++;\n"
        "}\n"                   // Line 10
    );

    // 3.3.7 Class scope (par. 1), part of point 5
    QTest::newRow("staticMemberVariableFollowingScopeOperator") << _(
        "struct C\n"
        "{\n"
        "    static int $member;\n"
        "};\n"
        "\n"                        // Line 5
        "int main()\n"
        "{\n"
        "    C::@member++;\n"
        "}\n"
    );

    // 3.3.7 Class scope (par. 2), from text
    QTest::newRow("staticMemberVariableFollowingDotOperator") << _(
        "struct C\n"
        "{\n"
        "    static int $member;\n"
        "};\n"
        "\n"                        // Line 5
        "int main()\n"
        "{\n"
        "    C c;\n"
        "    c.@member;\n"
        "}\n"                       // Line 10
    );


    // 3.3.7 Class scope (par. 2), from text
    QTest::newRow("staticMemberVariableFollowingArrowOperator") << _(
        "struct C\n"
        "{\n"
        "    static int $member;\n"
        "};\n"
        "\n"                         // Line 5
        "int main()\n"
        "{\n"
        "    C *c;\n"
        "    c->@member++;\n"
        "}\n"                        // Line 10
    );

    // 3.3.8 Enumeration scope
    QTest::newRow("previouslyDefinedEnumValueFromInsideEnum") << _(
        "enum {\n"
        "    $i = 0,\n"
        "    j = @i // refers to i above\n"
        "};\n"
    );

    // 3.3.8 Enumeration scope
    QTest::newRow("nsMemberHidesNsMemberIntroducedByUsingDirective") << _(
        "namespace A {\n"
        "  char x;\n"
        "}\n"
        "\n"
        "namespace B {\n"                               // Line 5
        "  using namespace A;\n"
        "  int $x; // hides A::x\n"
        "}\n"
        "\n"
        "int main()\n"                                  // Line 10
        "{\n"
        "    B::@x++; // refers to B's X, not A::x\n"
        "}\n"
    );

    // 3.3.10 Name hiding, from text
    // www.stroustrup.com/bs_faq2.html#overloadderived
    QTest::newRow("baseClassFunctionIntroducedByUsingDeclaration") << _(
        "struct B {\n"
        "    int $f(int) {}\n"
        "};\n"
        "\n"
        "class D : public B {\n"                              // Line 5
        "public:\n"
        "    using B::f; // make every f from B available\n"
        "    double f(double) {}\n"
        "};\n"
        "\n"                                                  // Line 10
        "int main()\n"
        "{\n"
        "    D* pd = new D;\n"
        "    pd->@f(2); // refers to B::f\n"
        "    pd->f(2.3); // refers to D::f\n"                 // Line 15
        "}\n"
    );

    // 3.3.10 Name hiding, from text
    // www.stroustrup.com/bs_faq2.html#overloadderived
    QTest::newRow("funWithSameNameAsBaseClassFunIntroducedByUsingDeclaration") << _(
        "struct B {\n"
        "    int f(int) {}\n"
        "};\n"
        "\n"
        "class D : public B {\n"                              // Line 5
        "public:\n"
        "    using B::f; // make every f from B available\n"
        "    double $f(double) {}\n"
        "};\n"
        "\n"                                                  // Line 10
        "int main()\n"
        "{\n"
        "    D* pd = new D;\n"
        "    pd->f(2); // refers to B::f\n"
        "    pd->@f(2.3); // refers to D::f\n"                // Line 15
        "}\n"
    );

    // 3.3.10 Name hiding (par 2.), from text
    // A class name (9.1) or enumeration name (7.2) can be hidden by the name of a variable,
    // data member, function, or enumerator declared in the same scope.
    QTest::newRow("funLocalVarHidesOuterClass") << _(
        "struct C {};\n"
        "\n"
        "int main()\n"
        "{\n"
        "    int $C; // hides type C\n"  // Line 5
        "    ++@C;\n"
        "}\n"
    );

    QTest::newRow("classConstructor") << _(
            "class Foo {\n"
            "    F@oo();"
            "    ~Foo();"
            "};\n\n"
            "Foo::$Foo()\n"
            "{\n"
            "}\n\n"
            "Foo::~Foo()\n"
            "{\n"
            "}\n"
    );

    QTest::newRow("classDestructor") << _(
            "class Foo {\n"
            "    Foo();"
            "    ~@Foo();"
            "};\n\n"
            "Foo::Foo()\n"
            "{\n"
            "}\n\n"
            "Foo::~$Foo()\n"
            "{\n"
            "}\n"
    );

    QTest::newRow("skipForwardDeclarationBasic") << _(
            "class $Foo {};\n"
            "class Foo;\n"
            "@Foo foo;\n"
    );

    QTest::newRow("skipForwardDeclarationTemplates") << _(
            "template <class E> class $Container {};\n"
            "template <class E> class Container;\n"
            "@Container<int> container;\n"
    );

    QTest::newRow("using_QTCREATORBUG7903_globalNamespace") << _(
            "namespace NS {\n"
            "class Foo {};\n"
            "}\n"
            "using NS::$Foo;\n"
            "void fun()\n"
            "{\n"
            "    @Foo foo;\n"
            "}\n"
    );

    QTest::newRow("using_QTCREATORBUG7903_namespace") << _(
            "namespace NS {\n"
            "class Foo {};\n"
            "}\n"
            "namespace NS1 {\n"
            "void fun()\n"
            "{\n"
            "    using NS::$Foo;\n"
            "    @Foo foo;\n"
            "}\n"
            "}\n"
    );

    QTest::newRow("using_QTCREATORBUG7903_insideFunction") << _(
            "namespace NS {\n"
            "class Foo {};\n"
            "}\n"
            "void fun()\n"
            "{\n"
            "    using NS::$Foo;\n"
            "    @Foo foo;\n"
            "}\n"
    );
}

void CppEditorPlugin::test_FollowSymbolUnderCursor()
{
    QFETCH(QByteArray, source);
    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, singleDocument(source));
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_multipleDocuments_data()
{
    QTest::addColumn<QList<TestDocumentPtr> >("documents");

    QTest::newRow("skipForwardDeclarationBasic") << (QList<TestDocumentPtr>()
        << TestDocument::create("class $Foo {};\n",
                                "defined.h")
        << TestDocument::create("class Foo;\n"
                                "@Foo foo;\n",
                                "forwardDeclaredAndUsed.h")
    );

    QTest::newRow("skipForwardDeclarationTemplates") << (QList<TestDocumentPtr>()
        << TestDocument::create("template <class E> class $Container {};\n",
                                "defined.h")
        << TestDocument::create("template <class E> class Container;\n"
                                "@Container<int> container;\n",
                                "forwardDeclaredAndUsed.h")
    );
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_multipleDocuments()
{
    QFETCH(QList<TestDocumentPtr>, documents);
    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, documents);
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_QObject_connect_data()
{
#define TAG(str) secondQObjectParam ? str : str ", no 2nd QObject"
    QTest::addColumn<char>("start");
    QTest::addColumn<char>("target");
    QTest::addColumn<bool>("secondQObjectParam");
    for (int i = 0; i < 2; ++i) {
        bool secondQObjectParam = (i == 0);
        QTest::newRow(TAG("SIGNAL: before keyword"))
                << '1' << '1' << secondQObjectParam;
        QTest::newRow(TAG("SIGNAL: in keyword"))
                << '2' << '1' << secondQObjectParam;
        QTest::newRow(TAG("SIGNAL: before parenthesis"))
                << '3' << '1' << secondQObjectParam;
        QTest::newRow(TAG("SIGNAL: before identifier"))
                << '4' << '1' << secondQObjectParam;
        QTest::newRow(TAG("SIGNAL: in identifier"))
                << '5' << '1' << secondQObjectParam;
        QTest::newRow(TAG("SIGNAL: before identifier parenthesis"))
                << '6' << '1' << secondQObjectParam;
        QTest::newRow(TAG("SLOT: before keyword"))
                << '7' << '2' << secondQObjectParam;
        QTest::newRow(TAG("SLOT: in keyword"))
                << '8' << '2' << secondQObjectParam;
        QTest::newRow(TAG("SLOT: before parenthesis"))
                << '9' << '2' << secondQObjectParam;
        QTest::newRow(TAG("SLOT: before identifier"))
                << 'A' << '2' << secondQObjectParam;
        QTest::newRow(TAG("SLOT: in identifier"))
                << 'B' << '2' << secondQObjectParam;
        QTest::newRow(TAG("SLOT: before identifier parenthesis"))
                << 'C' << '2' << secondQObjectParam;
    }
#undef TAG
}

static void selectMarker(QByteArray *source, char marker, char number)
{
    int idx = 0;
    forever {
        idx = source->indexOf(marker, idx);
        if (idx == -1)
            break;
        if (source->at(idx + 1) == number) {
            ++idx;
            source->remove(idx, 1);
        } else {
            source->remove(idx, 2);
        }
    }
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_QObject_connect()
{
    QFETCH(char, start);
    QFETCH(char, target);
    QFETCH(bool, secondQObjectParam);
    QByteArray source =
            "class Foo : public QObject\n"
            "{\n"
            "signals:\n"
            "    void $1endOfWorld();\n"
            "public slots:\n"
            "    void $2onWorldEnded()\n"
            "    {\n"
            "    }\n"
            "};\n"
            "\n"
            "void bla()\n"
            "{\n"
            "   Foo foo;\n"
            "   connect(&foo, @1SI@2GNAL@3(@4end@5OfWorld@6()),\n"
            "           &foo, @7SL@8OT@9(@Aon@BWorldEnded@C()));\n"
            "}\n";

    selectMarker(&source, '@', start);
    selectMarker(&source, '$', target);

    if (!secondQObjectParam)
        source.replace(" &foo, ", QByteArray());

    if (start >= '7' && !secondQObjectParam) {
        qWarning("SLOT jump triggers QTCREATORBUG-10265. Skipping.");
        return;
    }

    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, singleDocument(source));
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_classOperator_onOperatorToken_data()
{
    QTest::addColumn<bool>("toDeclaration");
    QTest::newRow("forward") << false;
    QTest::newRow("backward") << true;
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_classOperator_onOperatorToken()
{
    QFETCH(bool, toDeclaration);

    QByteArray source =
            "class Foo {\n"
            "    void @operator[](size_t idx);\n"
            "};\n\n"
            "void Foo::$operator[](size_t idx)\n"
            "{\n"
            "}\n";
    if (toDeclaration)
        source.replace('@', '#').replace('$', '@').replace('#', '$');
    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, singleDocument(source));
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_classOperator_data()
{
    test_FollowSymbolUnderCursor_classOperator_onOperatorToken_data();
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_classOperator()
{
    QFETCH(bool, toDeclaration);

    QByteArray source =
            "class Foo {\n"
            "    void $2operator@1[](size_t idx);\n"
            "};\n\n"
            "void Foo::$1operator@2[](size_t idx)\n"
            "{\n"
            "}\n";
    if (toDeclaration)
        source.replace("@1", QByteArray()).replace("$1", QByteArray())
                .replace("@2", "@").replace("$2", "$");
    else
        source.replace("@2", QByteArray()).replace("$2", QByteArray())
                .replace("@1", "@").replace("$1", "$");
    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, singleDocument(source));
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_classOperator_inOp_data()
{
    test_FollowSymbolUnderCursor_classOperator_onOperatorToken_data();
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_classOperator_inOp()
{
    QFETCH(bool, toDeclaration);

    QByteArray source =
            "class Foo {\n"
            "    void $2operator[@1](size_t idx);\n"
            "};\n\n"
            "void Foo::$1operator[@2](size_t idx)\n"
            "{\n"
            "}\n";
    if (toDeclaration)
        source.replace("@1", QByteArray()).replace("$1", QByteArray())
                .replace("@2", "@").replace("$2", "$");
    else
        source.replace("@2", QByteArray()).replace("$2", QByteArray())
                .replace("@1", "@").replace("$1", "$");
    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, singleDocument(source));
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_virtualFunctionCall_data()
{
    QTest::addColumn<QByteArray>("source");
    QTest::addColumn<OverrideItemList>("results");

    /// Check: Static type is base class pointer, all overrides are presented.
    QTest::newRow("allOverrides") << _(
            "struct A { virtual void virt() = 0; };\n"
            "void A::virt() {}\n"
            "\n"
            "struct B : A { void virt(); };\n"
            "void B::virt() {}\n"
            "\n"
            "struct C : B { void virt(); };\n"
            "void C::virt() {}\n"
            "\n"
            "struct CD1 : C { void virt(); };\n"
            "void CD1::virt() {}\n"
            "\n"
            "struct CD2 : C { void virt(); };\n"
            "void CD2::virt() {}\n"
            "\n"
            "int f(A *o) { o->$@virt(); }\n"
            "}\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("A::virt = 0"), 2)
            << OverrideItem(QLatin1String("B::virt"), 5)
            << OverrideItem(QLatin1String("C::virt"), 8)
            << OverrideItem(QLatin1String("CD1::virt"), 11)
            << OverrideItem(QLatin1String("CD2::virt"), 14));

    /// Check: Static type is derived class pointer, only overrides of sub classes are presented.
    QTest::newRow("possibleOverrides1") << _(
            "struct A { virtual void virt() = 0; };\n"
            "void A::virt() {}\n"
            "\n"
            "struct B : A { void virt(); };\n"
            "void B::virt() {}\n"
            "\n"
            "struct C : B { void virt(); };\n"
            "void C::virt() {}\n"
            "\n"
            "struct CD1 : C { void virt(); };\n"
            "void CD1::virt() {}\n"
            "\n"
            "struct CD2 : C { void virt(); };\n"
            "void CD2::virt() {}\n"
            "\n"
            "int f(B *o) { o->$@virt(); }\n"
            "}\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("B::virt"), 5)
            << OverrideItem(QLatin1String("C::virt"), 8)
            << OverrideItem(QLatin1String("CD1::virt"), 11)
            << OverrideItem(QLatin1String("CD2::virt"), 14));

    /// Check: Virtual function call in member of class hierarchy,
    ///        only possible overrides are presented.
    QTest::newRow("possibleOverrides2") << _(
            "struct A { virtual void virt(); };\n"
            "void A::virt() {}\n"
            "\n"
            "struct B : public A { void virt(); };\n"
            "void B::virt() {}\n"
            "\n"
            "struct C : public B { void g() { virt$@(); } }; \n"
            "\n"
            "struct D : public C { void virt(); };\n"
            "void D::virt() {}\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("B::virt"), 5)
            << OverrideItem(QLatin1String("D::virt"), 10));

    /// Check: If no definition is found, fallback to the declaration.
    QTest::newRow("fallbackToDeclaration") << _(
            "struct A { virtual void virt(); };\n"
            "\n"
            "int f(A *o) { o->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("A::virt"), 1));

    /// Check: Ensure that the first entry in the final results is the same as the first in the
    ///        immediate results.
    QTest::newRow("itemOrder") << _(
            "struct C { virtual void virt() = 0; };\n"
            "void C::virt() {}\n"
            "\n"
            "struct B : C { void virt(); };\n"
            "void B::virt() {}\n"
            "\n"
            "struct A : B { void virt(); };\n"
            "void A::virt() {}\n"
            "\n"
            "int f(C *o) { o->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("C::virt = 0"), 2)
            << OverrideItem(QLatin1String("A::virt"), 8)
            << OverrideItem(QLatin1String("B::virt"), 5));

    /// Check: If class templates are involved, the class and function symbols might be generated.
    ///        In that case, make sure that the symbols are not deleted before we reference them.
    QTest::newRow("instantiatedSymbols") << _(
            "template <class T> struct A { virtual void virt() {} };\n"
            "void f(A<int> *l) { l->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("A::virt"), 1));

    /// Check: Static type is nicely resolved, especially for QSharedPointers.
    QTest::newRow("QSharedPointer") << _(
            "template <class T>\n"
            "class Basic\n"
            "{\n"
            "public:\n"
            "    inline T &operator*() const;\n"
            "    inline T *operator->() const;\n"
            "};\n"
            "\n"
            "template <class T> class ExternalRefCount: public Basic<T> {};\n"
            "template <class T> class QSharedPointer: public ExternalRefCount<T> {};\n"
            "\n"
            "struct A { virtual void virt() {} };\n"
            "struct B : public A { void virt() {} };\n"
            "\n"
            "int f()\n"
            "{\n"
            "    QSharedPointer<A> p(new A);\n"
            "    p->$@virt();\n"
            "}\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("A::virt"), 12)
            << OverrideItem(QLatin1String("B::virt"), 13));

    /// Check: In case there is no override for the static type of a function call expression,
    ///        make sure to:
    ///         1) include the last provided override (look up bases)
    ///         2) and all overrides whose classes are derived from that static type
    QTest::newRow("noSiblings_references") << _(
            "struct A { virtual void virt(); };\n"
            "struct B : A { void virt() {} };\n"
            "struct C1 : B { void virt() {} };\n"
            "struct C2 : B { };\n"
            "struct D : C2 { void virt() {} };\n"
            "\n"
            "void f(C2 &o) { o.$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("B::virt"), 2)
            << OverrideItem(QLatin1String("D::virt"), 5));

    /// Variation of noSiblings_references
    QTest::newRow("noSiblings_pointers") << _(
            "struct A { virtual void virt(); };\n"
            "struct B : A { void virt() {} };\n"
            "struct C1 : B { void virt() {} };\n"
            "struct C2 : B { };\n"
            "struct D : C2 { void virt() {} };\n"
            "\n"
            "void f(C2 *o) { o->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("B::virt"), 2)
            << OverrideItem(QLatin1String("D::virt"), 5));

    /// Variation of noSiblings_references
    QTest::newRow("noSiblings_noBaseExpression") << _(
            "struct A { virtual void virt() {} };\n"
            "struct B : A { void virt() {} };\n"
            "struct C1 : B { void virt() {} };\n"
            "struct C2 : B { void g() { $@virt(); } };\n"
            "struct D : C2 { void virt() {} };\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("B::virt"), 2)
            << OverrideItem(QLatin1String("D::virt"), 5));

    /// Check: Trigger on a.virt() if a is of type &A.
    QTest::newRow("onDotMemberAccessOfReferenceTypes") << _(
            "struct A { virtual void virt() = 0; };\n"
            "void A::virt() {}\n"
            "\n"
            "void client(A &o) { o.$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("A::virt = 0"), 2));

    /// Check: Do not trigger on a.virt() if a is of type A.
    QTest::newRow("notOnDotMemberAccessOfNonReferenceType") << _(
            "struct A { virtual void virt(); };\n"
            "void A::$virt() {}\n"
            "\n"
            "void client(A o) { o.@virt(); }\n")
        << OverrideItemList();

    /// Check: Do not trigger on qualified function calls.
    QTest::newRow("notOnQualified") << _(
            "struct A { virtual void virt(); };\n"
            "void A::$virt() {}\n"
            "\n"
            "struct B : public A {\n"
            "    void virt();\n"
            "    void g() { A::@virt(); }\n"
            "};\n")
        << OverrideItemList();

    /// Check: Do not trigger on member function declaration.
    QTest::newRow("notOnDeclaration") << _(
            "struct A { virtual void virt(); };\n"
            "void A::virt() {}\n"
            "\n"
            "struct B : public A { void virt@(); };\n"
            "void B::$virt() {}\n")
        << OverrideItemList();

    /// Check: Do not trigger on function definition.
    QTest::newRow("notOnDefinition") << _(
            "struct A { virtual void virt(); };\n"
            "void A::virt() {}\n"
            "\n"
            "struct B : public A { void $virt(); };\n"
            "void B::@virt() {}\n")
        << OverrideItemList();

    QTest::newRow("notOnNonPointerNonReference") << _(
            "struct A { virtual void virt(); };\n"
            "void A::virt() {}\n"
            "\n"
            "struct B : public A { void virt(); };\n"
            "void B::$virt() {}\n"
            "\n"
            "void client(B b) { b.@virt(); }\n")
        << OverrideItemList();

    QTest::newRow("differentReturnTypes") << _(
            "struct Base { virtual Base *virt() { return this; } };\n"
            "struct Derived : public Base { Derived *virt() { return this; } };\n"
            "void client(Base *b) { b->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("Base::virt"), 1)
            << OverrideItem(QLatin1String("Derived::virt"), 2));

    QTest::newRow("QTCREATORBUG-10294_cursorIsAtTheEndOfVirtualFunctionName") << _(
            "struct Base { virtual void virt() {} };\n"
            "struct Derived : Base { void virt() {} };\n"
            "void client(Base *b) { b->virt$@(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("Base::virt"), 1)
            << OverrideItem(QLatin1String("Derived::virt"), 2));

    QTest::newRow("static_call") << _(
            "struct Base { virtual void virt() {} };\n"
            "struct Derived : Base { void virt() {} };\n"
            "struct Foo {\n"
            "    static Base *base();\n"
            "};\n"
            "void client() { Foo::base()->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("Base::virt"), 1)
            << OverrideItem(QLatin1String("Derived::virt"), 2));

    QTest::newRow("double_call") << _(
            "struct Base { virtual void virt() {} };\n"
            "struct Derived : Base { void virt() {} };\n"
            "struct Foo { Base *base(); };\n"
            "Foo *instance();\n"
            "void client() { instance()->base()->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("Base::virt"), 1)
            << OverrideItem(QLatin1String("Derived::virt"), 2));

    QTest::newRow("casting") << _(
            "struct Base { virtual void virt() {} };\n"
            "struct Derived : Base { void virt() {} };\n"
            "void client() { static_cast<Base *>(0)->$@virt(); }\n")
        << (OverrideItemList()
            << OverrideItem(QLatin1String("Base::virt"), 1)
            << OverrideItem(QLatin1String("Derived::virt"), 2));
}

void CppEditorPlugin::test_FollowSymbolUnderCursor_virtualFunctionCall()
{
    QFETCH(QByteArray, source);
    QFETCH(OverrideItemList, results);

    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, singleDocument(source), results);
}

/// Check: Base classes can be found although these might be defined in distinct documents.
void CppEditorPlugin::test_FollowSymbolUnderCursor_virtualFunctionCall_multipleDocuments()
{
    QList<TestDocumentPtr> testFiles = QList<TestDocumentPtr>()
            << TestDocument::create("struct A { virtual void virt(int) = 0; };\n",
                                    "a.h")
            << TestDocument::create("#include \"a.h\"\n"
                                    "struct B : A { void virt(int) {} };\n",
                                    "b.h")
            << TestDocument::create("#include \"a.h\"\n"
                                    "void f(A *o) { o->$@virt(42); }\n",
                                    "u.cpp")
            ;

    const OverrideItemList finalResults = OverrideItemList()
            << OverrideItem(QLatin1String("A::virt = 0"), 1)
            << OverrideItem(QLatin1String("B::virt"), 2);

    F2TestCase(F2TestCase::FollowSymbolUnderCursorAction, testFiles, finalResults);
}

/*
Potential test cases improving name lookup.

If you fix one, add a test and remove the example from here.

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.1 Declarative regions and scopes, copy-paste (main added)
int j = 24;
int main()
{
    int i = j, j; // First j refers to global j, second j refers to just locally defined j
    j = 42; // Refers to locally defined j
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.2 Point of declaration (par. 2), copy-paste (main added)
const int i = 2;
int main()
{
    int i[i]; // Second i refers to global
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.2 Point of declaration (par. 9), copy-paste (main added)
typedef unsigned char T;
template<class T
= T // lookup finds the typedef name of unsigned char
, T // lookup finds the template parameter
N = 0> struct A { };

int main() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.9 Template parameter scope (par. 3), copy-paste (main added), part 1
template<class T, T* p, class U = T> class X {}; // second and third T refers to first one
template<class T> void f(T* p = new T);

int main() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.9 Template parameter scope (par. 3), copy-paste (main added), part 2
template<class T> class X : public Array<T> {};
template<class T> class Y : public T {};

int main() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.9 Template parameter scope (par. 4), copy-paste (main added), part 2
typedef int N;
template<N X, typename N, template<N Y> class T> struct A; // N refers to N above

int main() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.10 Name hiding (par 1.), from text, example 2a

// www.stroustrup.com/bs_faq2.html#overloadderived
// "In C++, there is no overloading across scopes - derived class scopes are not
// an exception to this general rule. (See D&E or TC++PL3 for details)."
struct B {
    int f(int) {}
};

struct D : public B {
    double f(double) {} // hides B::f
};

int main()
{
    D* pd = new D;
    pd->f(2); // refers to D::f
    pd->f(2.3); // refers to D::f
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 3.3.10 Name hiding (par 2.), from text
int C; // hides following type C, order is not important
struct C {};

int main()
{
    ++C;
}

*/
