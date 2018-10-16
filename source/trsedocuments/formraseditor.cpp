/*
 * Turbo Rascal Syntax error, “;” expected but “BEGIN” (TRSE, Turbo Rascal SE)
 * 8 bit software development IDE for the Commodore 64
 * Copyright (C) 2018  Nicolaas Ervik Groeneboom (nicolaas.groeneboom@gmail.com)
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (LICENSE.txt).
 *   If not, see <https://www.gnu.org/licenses/>.
*/

#include "formraseditor.h"
#include "ui_formraseditor.h"
#include <QProcess>
#include "source/Compiler/errorhandler.h"
#include <QElapsedTimer>
#include "source/LeLib/util/util.h"


FormRasEditor::FormRasEditor(QWidget *parent) :
    TRSEDocument(parent),
    ui(new Ui::FormRasEditor)
{
    ui->setupUi(this);
    m_fileExtension = "ras";
}

FormRasEditor::~FormRasEditor()
{
    delete ui;
}

void FormRasEditor::ExecutePrg(QString fileName, QString emulator, QString system)
{
    if (!QFile::exists(emulator)) {
        Messages::messages.DisplayMessage(Messages::messages.NO_EMULATOR);
        return;
    }
    QProcess process;

    QStringList params;
    if (system=="VIC20")
         params<< "-autostartprgmode" << "1";

    params << QDir::toNativeSeparators(fileName);
    process.waitForFinished();
#ifdef _WIN32
    QProcess::execute("taskkill /im \"x64.exe\" /f");
#endif
//    qDebug() << emulator << " " << params <<  QDir::toNativeSeparators(fileName);
    process.startDetached(emulator, params);
//    process.pi
    QString output(process.readAllStandardOutput());

}

void FormRasEditor::InitDocument(WorkerThread *t, CIniFile *ini, CIniFile* pro)
{
    TRSEDocument::InitDocument(t,ini, pro);
    FillFromIni();

    setupEditor();
}



void FormRasEditor::setupEditor()
{
    m_font.setFamily("Courier");
    m_font.setFixedPitch(true);
    m_font.setPointSize(m_iniFile->getdouble("font_size"));
    //ui->txtEditor->setTextColor(QColor(220,210,190));
    SetupHighlighter();
//    highlighter->Save("dark_standard.ini");

    UpdateFromIni();
//    ui->txtEditor->setTabStopWidth(m_iniFile->getInt("tab_width") * metrics.width(' '));

}

void FormRasEditor::Compress()
{

}

void FormRasEditor::Build()
{
    SaveCurrent();
    emit requestBuild();
    if (BuildStep())
        {
        compiler.SaveBuild(filename + ".asm");
        QString text ="Build <b><font color=\"#90FF90\">Successful</font>!</b> ( "+  (Util::MilisecondToString(timer.elapsed())) +")<br>";
        text+="Assembler file saved to : <b>" + filename+".asm</b><br>";
        text+="Compiled <b>" + QString::number(parser.m_lexer->m_lines.count()) +"</b> lines of Turbo Rascal to <b>";
        text+=QString::number(compiler.m_assembler->getLineCount()) + "</b> lines of assembler instructions (and variables/labels)<br>";
        if (m_iniFile->getdouble("post_optimize")==1) {
            text+="Post-optimized away <b>" + QString::number(compiler.m_assembler->m_totalOptimizedLines) +"</b> lines of assembler instructions ";
            text=text+"(<font color=\"#70FF40\"> " + QString::number((int)(100.0*(float)compiler.m_assembler->m_totalOptimizedLines/(float)compiler.m_assembler->getLineCount()))+  " % </font> of total ) <br>";

        }
        else
            text+="<font color=\"#FFA090\">Warning:</font>Post-optimizer disabled. Enable for faster results (unless post-optimizer breaks something).<br>";

//        text+="+"<br>";
        QString output;

        int codeEnd = 0;
        qDebug() << m_iniFile->getString("assembler");
        if (m_iniFile->getString("assembler").toLower()=="dasm") {
            QProcess process;
            process.start(m_iniFile->getString("dasm"), QStringList()<<(filename +".asm") << ("-o"+filename+".prg") << "-v3");
            process.waitForFinished();
            //process;
            //qDebug() <<process.readAllStandardOutput();
            output = process.readAllStandardOutput();

            codeEnd=FindEndSymbol(output);
        }
        else if (m_iniFile->getString("assembler").toLower()=="orgasm") {
            Orgasm2 orgAsm;
            //orgAsm.LoadCodes();
            orgAsm.Assemble(filename+".asm", filename+".prg");
            if (orgAsm.m_success) {
                output = "complete.";
                //for (QString s : orgAsm.m_debug)
                //qDebug() << s ;
            }
        }
        // Machine Code Analyzer
        VerifyMachineCodeZP(filename+".prg");



//        qDebug() << "Code end: " << Util::numToHex(codeEnd) << codeEnd;
        int orgFileSize = QFile(filename+".prg").size();

        if (m_iniFile->getdouble("perform_crunch")==1) {
            QProcess processCompress;

            QString fn = (filename +".prg");
            if (!QFile::exists(m_iniFile->getString("exomizer")))
                Messages::messages.DisplayMessage(Messages::messages.NO_EXOMIZER);
            QStringList exoParams = QStringList() << "sfx" << "$0810" << fn<< "-o" << fn;
            if (m_iniFile->getdouble("hide_exomizer_footprint")==1)
                exoParams << "-n";
            processCompress.start(m_iniFile->getString("exomizer"), exoParams  );
            processCompress.waitForFinished();
        }
        int size = QFile(filename+".prg").size();


        m_buildSuccess = true;

        if (output.toLower().contains("error")) {
            text="<font color=\"#FF6040\">DASM Fatal error!</font><br>";
            m_buildSuccess = false;
            if (output.toLower().contains("branch out of range")) {
                Messages::messages.DisplayMessage(Messages::messages.BRANCH_ERROR);
                output += "<br>Please check your <b>onpage/offpage</b> keywords.";

            }
            else
            if (output.toLower().contains("reverse-indexed")) {
                Messages::messages.DisplayMessage(Messages::messages.MEMORY_OVERLAP_ERROR);
                output += "<br>Please reorganize your binary inclusions in ascending order of memory locations.";
            }
            else
            if (output.toLower().contains("mnemonic")) {
                output += "<br>Please make sure you have used well-defined labels and variables in your inline assembly code.";
            }

            else
                Messages::messages.DisplayMessage(Messages::messages.DASM_COMPILER_ERROR);

        }
        if (!output.toLower().contains("complete.")) {
            m_buildSuccess = false;
            if (output=="") {
                Messages::messages.DisplayMessage(Messages::messages.NO_DASM);

                output = output + "\nCould not find Dasm.exe. Did you set the correct environment variables?";
            }

        }


        if (m_buildSuccess) {
            output ="Assembled file size: <b>" + QString::number(size) + "</b> bytes";
            if (m_iniFile->getdouble("perform_crunch")==1) {
                output=output+" (<font color=\"#70FF40\"> " + QString::number((int)(100.0*(float)size/(float)orgFileSize))+  " % </font> of original size ) <br>";
                output=output+"Original file size: " + QString::number(orgFileSize) + " bytes";
            }

        }
        TestForCodeOverwrite(codeEnd,text);

        //ui->ed
        ui->txtOutput->setText(text + output);
        ui->txtEditor->m_cycles =  compiler.m_assembler->m_cycles;
        ui->txtEditor->RepaintCycles();
        ui->txtEditor->InitCompleter(compiler.m_assembler->m_symTab, &parser);

    }
    else {
        SetOutputText(ErrorHandler::e.m_teOut);
        m_outputText = ErrorHandler::e.m_teOut;
        int ln = Pmm::Data::d.lineNumber;

        emit OpenOtherFile(compiler.recentError.file, ln);
        GotoLine(ln);
        m_buildSuccess = false;

    }
    if (m_projectIniFile->getString("system")=="NES") {
        BuildNes(m_currentSourceFile.split(".")[0]);

    }
    SetLights();
}

void FormRasEditor::SetOutputText(QString txt)
{
    ui->txtOutput->setText(ErrorHandler::e.m_teOut);

}

int FormRasEditor::FindEndSymbol(QString out)
{
    QStringList output = QString(out).split("\n");
    for (QString s : output) {
        if (s.toLower().contains("endsymbol")) {
            s= s.remove("EndSymbol").trimmed();
            bool ok;
            return s.toInt(&ok, 16);
        }
    }
    return 0;
}

void FormRasEditor::FindBlockEndSymbols(QString out)
{
    m_blockEndSymbols.clear();
    QStringList output = QString(out).split("\n");
    for (QString s : output) {
        if (s.toLower().contains("endblock")) {
            s = s.toLower().simplified().split(" ")[1];

            bool ok;
            int i= s.toInt(&ok, 16);
            m_blockEndSymbols.append(i);
        }
    }

}

void FormRasEditor::ConnectBlockSymbols()
{
    for (int sym : m_blockEndSymbols) {
        int winner = 0xFFFF;
        MemoryBlock* winnerBlock=nullptr;

        for (MemoryBlock* mb: compiler.m_assembler->blocks) {
//            if (mb->m_type==MemoryBlock::CODE &&  sym>mb->m_start)
                if (sym>mb->m_start)
                if (sym-mb->m_start<winner) {
                    winner = sym-mb->m_start;
                    winnerBlock  =mb;
                }
        }
        if (winnerBlock!=nullptr) {
            winnerBlock->m_end = sym;
       //     qDebug() << Util::numToHex(sym) << " " << winnerBlock->Type();
        }
    }


}

void FormRasEditor::BuildNes(QString prg)
{
    QByteArray header;
    header.resize(16);
    header.fill(0);
    header[0] = 0x4E;
    header[1] = 0x45;
    header[2] = 0x53;
    header[3] = 0x1A;
    // 0000 1000

    header[4] = 2; // PRG rom kb
    // 0001 0000

    header[5] = 0; // CHR rom
    header[6] = 0b00000001;



    QFile f(prg+ ".prg");
    f.open(QFile::ReadOnly);
    QByteArray data = f.readAll();
    f.close();
//    qDebug() << prg;

    QFile out(prg+ ".nes");
    out.open(QFile::WriteOnly);
    out.write(header);
    out.write(data);
    out.close();

}

bool FormRasEditor::VerifyMachineCodeZP(QString fname)
{

    m_mca.Load(fname);
    m_mca.AnalyzeZP();
    return true;
}



void FormRasEditor::Setup()
{
      setupEditor();
}

void FormRasEditor::Run()
{
    if (!m_buildSuccess)
        return;
    QString filename = m_currentSourceFile.split(".")[0] + ".prg";

    QString emulator = "";
    if (Syntax::s.m_currentSystem==Syntax::C64)
        emulator = m_iniFile->getString("emulator");
    if (Syntax::s.m_currentSystem==Syntax::VIC20)
        emulator = m_iniFile->getString("vic20_emulator");
    if (Syntax::s.m_currentSystem==Syntax::NES) {
        emulator = m_iniFile->getString("nes_emulator");
        filename = m_currentSourceFile.split(".")[0] + ".nes";
    }
    if (emulator=="")
        return;
    ExecutePrg(filename,emulator, m_projectIniFile->getString("system"));

}

void FormRasEditor::SetLights()
{
    if (!m_buildSuccess)
        ui->lblLight->setStyleSheet("QLabel { background-color : red; color : blue; }");
    else
        ui->lblLight->setStyleSheet("QLabel { background-color : green; color : blue; }");


}

void FormRasEditor::SetText(QString s)
{
    ui->txtEditor->setPlainText(s);
    ui->txtEditor->m_textChanged = false;
}

void FormRasEditor::SetupHighlighter()
{
    if (highlighter != nullptr)
        delete highlighter;
    CIniFile colors;
    colors.Load(Util::path + "themes/" + m_iniFile->getString("theme"));
    ui->txtEditor->InitColors(colors);

    QPalette p = ui->txtEditor->palette();
    p.setColor(QPalette::Base, colors.getColor("backgroundcolor"));
    p.setColor(QPalette::Text, colors.getColor("textcolor"));
    ui->txtEditor->setPalette(p);
    highlighter = new Highlighter(colors, ui->txtEditor->document());

//    qDebug() << "UPDATE " << m_iniFile->getString("theme");

}


void FormRasEditor::wheelEvent(QWheelEvent *event)
{

}

void FormRasEditor::keyPressEvent(QKeyEvent *e)
{
    TRSEDocument::keyPressEvent(e);
    if (e->key() == Qt::Key_Escape && ui->leSearch->hasFocus()) {
        ui->txtEditor->setFocus();
    }

    if (e->key()==Qt::Key_W && (QApplication::keyboardModifiers() & Qt::ControlModifier))
        emit requestCloseWindow();
    //    Data::data.requestCloseWindow = true;

//    if (ui->txtEditor->m_textChanged)
        m_documentIsChanged  = ui->txtEditor->m_textChanged;

    if (e->key()==Qt::Key_K && (QApplication::keyboardModifiers() & Qt::ControlModifier))
        AutoFormat();

    if (e->key()==Qt::Key_F && QApplication::keyboardModifiers() & Qt::ControlModifier) {
        ui->leSearch->setText("");
        m_searchFromPos = ui->txtEditor->textCursor().position();
        ui->leSearch->setFocus();
    }

    if (e->key()==Qt::Key_F1) {
        QTextCursor tc = ui->txtEditor->textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        QString word = tc.selectedText();

        DialogHelp* dh = new DialogHelp(this, word, m_defaultPalette);
//        dh->setPalette(m_defaultPalette);
     //   QApplication::setPalette(m_defaultPalette);

        dh->show();

    }

    if (e->key()==Qt::Key_F2) {
        QTextCursor tc = ui->txtEditor->textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        QString word = tc.selectedText();
        for (Node*n : parser.m_proceduresOnly) {
            NodeProcedureDecl* np = dynamic_cast<NodeProcedureDecl*>(n);
            if (np->m_procName.toLower()==word.toLower()) {
                int ln=np->m_op.m_lineNumber;
                QTextCursor cursor(ui->txtEditor->document()->findBlockByLineNumber(ln));
                ui->txtEditor->setTextCursor(cursor);

            }
        }


    }


    if (e->key() == Qt::Key_U &&  (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        MemoryAnalyze();
    }
    if (e->key() == Qt::Key_R &&  (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        Build();
        Run();
    }



}

void FormRasEditor::TestForCodeOverwrite(int codeEnd, QString& output)
{
    for (MemoryBlock& mb: compiler.m_assembler->m_userWrittenBlocks) {
//        qDebug() << Util::numToHex(mb.m_start) << " vs " << Util::numToHex(codeEnd) ;
        if (mb.m_start<codeEnd && mb.m_start>=Syntax::s.m_startAddress) {
            output +="\n<font color=\"#FF8080\">WARNING:</font>Possible code block overwrite on line <b>" +QString::number(mb.m_lineNumber) + "</b>.&nbsp;";
            output += "<font color=\"#FF8080\">Proceed with caution </font>(writing to <font color=\"#FF8080\">"+Util::numToHex(mb.m_start)+"</font>, code ends at <font color=\"#FF8080\">"+Util::numToHex(codeEnd) +")</font>. <br>";
        }
    }
}

void FormRasEditor::GotoLine(int ln)
{
    QTextCursor cursor(ui->txtEditor->document()->findBlockByLineNumber(ln-1));
    ui->txtEditor->setTextCursor(cursor);
}



void FormRasEditor::on_leSearch_textChanged()
{
    QString i;
    SearchInSource();
}

void FormRasEditor::SearchInSource()
{
    m_currentFromPos = ui->txtEditor->document()->toPlainText().toLower().indexOf(ui->leSearch->text().toLower(), m_searchFromPos);
    QTextCursor cursor(ui->txtEditor->document()->findBlock(m_currentFromPos));
    ui->txtEditor->setTextCursor(cursor);
}

void FormRasEditor::UpdateColors()
{
    SetupHighlighter();
}

void FormRasEditor::UpdateFromIni()
{

    QFontMetrics metrics(m_font);

    m_font.setPointSize(m_iniFile->getdouble("font_size"));
    ui->txtEditor->setFont(m_font);
    ui->txtEditor->setTabStopWidth(m_iniFile->getInt("tab_width") * metrics.width(' '));


}

void FormRasEditor::AutoFormat()
{
    if (!BuildStep())
        return;

    int pos = ui->txtEditor->textCursor().position();
    QStringList source = ui->txtEditor->document()->toPlainText().split("\n");
    int ln = 0;
    int curBlock = 0;
    int nextBlock = 0;
    int singleLine = 0;
    QString outSource = "";
    bool varBlock = false;
    for (QString& s:source) {
        QString nxt = "";
        if (ln+1<m_currentSourceFile.count()-1)
            nxt = source[ln+1];
        nxt=nxt.toLower();

        int add = singleLine;
        s=s.trimmed();
        nextBlock = curBlock;
        QString k = s.toLower();
        if (k.startsWith("procedure")) {
            curBlock = 0;
            nextBlock = 0;
        }

        if (varBlock)
            if (k.startsWith("procedure") || k.startsWith("begin")) {
                varBlock = false;
                curBlock = 0;
                nextBlock = 0;
            }


        if (varBlock) {
            curBlock=1;
            nextBlock = 1;
        }
        if (k=="var")
            varBlock = true;

        if (k.contains("begin"))
            nextBlock++;
        if (k.startsWith("end")) {
            curBlock--;
            nextBlock = curBlock;
        }
        if (k.contains("if ") || k.contains("while ") || k.contains("for ") || k.contains("else")) {
            if (!k.contains("begin") && !nxt.contains("begin"))
                if (!k.endsWith(";") )
                singleLine = 1;
        }
        for (int i=0;i<curBlock+add;i++)
            s.insert(0,"\t");

        if (add!=0) {
            curBlock--;
            singleLine = 0;
        }

        ln++;
        outSource+=s+"\n";
        curBlock = nextBlock;
    }
    SetText(outSource);
    QTextCursor tc = ui->txtEditor->textCursor();
    tc.setPosition(pos);
    ui->txtEditor->setTextCursor(tc);

}

bool FormRasEditor::BuildStep()
{
    if (!m_currentSourceFile.toLower().endsWith(".ras")) {
        ui->txtOutput->setText("Turbo Rascal SE can only compile .ras files");
        m_buildSuccess = false;
        return false;
    }


    QString text = ui->txtEditor->toPlainText();
    ErrorHandler::e.m_level = ErrorHandler::e.ERROR_ONLY;
    ErrorHandler::e.m_teOut = "";
    ErrorHandler::e.exitOnError = false;
    QStringList lst = text.split("\n");
 //   text = text.replace("\n","");
//    SymbolTable::isInitialized = true;

    timer.start();
    lexer = Lexer(text, lst, m_projectIniFile->getString("project_path"));
    parser = Parser(&lexer);
    compiler = Compiler(&parser,m_iniFile, m_projectIniFile);
    compiler.Parse();
//    compiler.Interpret();

    //compiler.Build(compiler::PASCAL);
    //compiler.SaveBuild(m_outputFilename+".pmm");

    QString path = m_projectIniFile->getString("project_path") + "/";
    filename = m_currentSourceFile.split(".")[0];

    return compiler.Build(Compiler::MOS6502, path);
}

void FormRasEditor::FillFromIni()
{
    ui->chkPostOpt->setChecked(m_iniFile->getdouble("post_optimize")==1);
    ui->chkExomize->setChecked(m_iniFile->getdouble("perform_crunch")==1);
//    qDebug() << "FillFromIni" << m_iniFile->getdouble("perform_crunch");
    isInitialized=true;
}

void FormRasEditor::FillToIni()
{
    if (!isInitialized)
        return;
//    qDebug() << "FillToIni A" << m_iniFile->getdouble("perform_crunch");
    if (ui->chkPostOpt->isChecked())
        m_iniFile->setFloat("post_optimize",1);
    else
        m_iniFile->setFloat("post_optimize",0);

    if (ui->chkExomize->isChecked())
        m_iniFile->setFloat("perform_crunch",1);
    else
        m_iniFile->setFloat("perform_crunch",0);

  //  qDebug() << "FillToIni B" << m_iniFile->getdouble("perform_crunch");

    m_iniFile->Save();
}

void FormRasEditor::MemoryAnalyze()
{
    int i= m_iniFile->getdouble("perform_crunch");
    m_iniFile->setFloat("perform_crunch",0);
    Build();
    m_iniFile->setFloat("perform_crunch",i);
    QProcess process;
    process.start(m_iniFile->getString("dasm"), QStringList()<<(filename +".asm") << ("-o"+filename+".prg") << "-v3");
    process.waitForFinished();
    //process;
    QString output = process.readAllStandardOutput();
    int codeEnd=FindEndSymbol(output);

    if (compiler.m_assembler==nullptr)
        return;

    FindBlockEndSymbols(output);
    ConnectBlockSymbols();
    compiler.m_assembler->blocks.append(new MemoryBlock(Syntax::s.m_startAddress, codeEnd, MemoryBlock::CODE, "code"));

    m_mca.ClassifyZP(compiler.m_assembler->blocks);


/*    for (MemoryBlock& mb:compiler.m_assembler->blocks) {
        qDebug() << QString::number(mb.m_start,16) << " to " <<QString::number(mb.m_end,16) << " " << mb.Type();
    }*/
    DialogMemoryAnalyze* dma = new DialogMemoryAnalyze();
    dma->Initialize(compiler.m_assembler->blocks, m_iniFile->getInt("memory_analyzer_font_size"));
    dma->exec();

}

void FormRasEditor::Reload()
{
    if (!m_currentSourceFile.contains(".asm"))
        return;
    int pos = ui->txtEditor->textCursor().position();

    Load(m_currentSourceFile);
    QTextCursor tc = ui->txtEditor->textCursor();
    tc.setPosition(pos);
    ui->txtEditor->setTextCursor(tc);

}

void FormRasEditor::Save(QString filename)
{
    if (QFile::exists(filename))
        QFile::remove(filename);
    QString txt = ui->txtEditor->document()->toPlainText();
    QFile file(filename);
    if (file.open(QIODevice::ReadWrite))
    {
        QTextStream stream(&file);
        stream << txt;
    }
    file.close();
    m_iniFile->Save();
    ui->txtEditor->m_textChanged = false;
}

void FormRasEditor::Load(QString filename)
{
    QFile file(filename);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        SetText(file.readAll());
    }
    file.close();

}



void FormRasEditor::on_leSearch_returnPressed()
{
    m_searchFromPos=m_currentFromPos+1;
    SearchInSource();

}

void FormRasEditor::on_leSearch_textChanged(const QString &arg1)
{
    SearchInSource();
}

void FormRasEditor::on_btnReplace_clicked()
{
    QString orgstr = ui->leSearch->text();
    QString replacestr = ui->leReplace->text();

    QString source = ui->txtEditor->toPlainText();
    source = source.replace(orgstr, replacestr);

    SetText(source);

}

void FormRasEditor::on_chkExomize_stateChanged(int arg1)
{
    FillToIni();
}

void FormRasEditor::on_chkPostOpt_stateChanged(int arg1)
{
    FillToIni();
}
