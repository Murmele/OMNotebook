/*
------------------------------------------------------------------------------------
This file is part of OpenModelica.

Copyright (c) 1998-2005, Link�pings universitet,
Department of Computer and Information Science, PELAB
See also: www.ida.liu.se/projects/OpenModelica

All rights reserved.

(The new BSD license, see also
http://www.opensource.org/licenses/bsd-license.php)


Redistribution and use in source and binary forms, with or without
modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of Link�pings universitet nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

For more information about the Qt-library visit TrollTech:s webpage regarding
licence: http://www.trolltech.com/products/qt/licensing.html

------------------------------------------------------------------------------------
*/

/*! 
 * \file inputcell.cpp
 * \author Ingemar Axelsson and Anders Fernstr�m
 * \date 2005-10-27 (update)
 *
 * \brief Describes a inputcell.
 */

//STD Headers
#include <exception>
#include <stdexcept>
#include <sstream>

//QT Headers
#include <QtCore/QDir>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QGridLayout>
#include <QtGui/QKeyEvent>
#include <QtGui/QLineEdit>
#include <QtGui/QMessageBox>
#include <QtGui/QResizeEvent>
#include <QtGui/QFrame>
#include <QtGui/QTextFrame>

//IAEX Headers
#include "inputcell.h"
#include "treeview.h"
#include "stylesheet.h"
#include "commandcompletion.h"
#include "highlighterthread.h"



namespace IAEX
{
	/*! 
	 * \class SleeperThread
	 * \author Anders Ferstr�m
	 *
	 * \brief Extends QThread. A small trick to get access to protected
	 * function in QThread.
	 */
	class SleeperThread : public QThread
	{
	public:
		static void msleep(unsigned long msecs)
		{
			QThread::msleep(msecs);
		}
	};



	/*! 
	 * \class MyTextEdit
	 * \author Anders Ferstr�m
	 * \date 2005-11-01
	 *
	 * \brief Extends QTextEdit. Mostly so I can catch when a user
	 * clicks on the editor
	 */
	MyTextEdit::MyTextEdit(QWidget *parent)
		: QTextBrowser(parent),
		inCommand(false),
		stopHighlighter(false)
	{
	}

	MyTextEdit::~MyTextEdit()
	{
	}

	bool MyTextEdit::isStopingHighlighter()
	{
		return stopHighlighter;
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-01
	 * \date 2005-12-15 (update)
	 *
	 * Needed a signal to be emited when the user click on the cell.
	 *
	 * 2005-12-15 AF, set inCommand to false when clicking on the cell,
	 * otherwise the commandcompletion class want be reseted when 
	 * changing inputcells by clicking.
	 */
	void MyTextEdit::mousePressEvent(QMouseEvent *event)
	{
		stopHighlighter = false;
		inCommand = false;
		QTextEdit::mousePressEvent(event);
		emit clickOnCell();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-28
	 *
	 * \brief Handles mouse wheel events, ignore them and send the up
	 * in the cell hierarchy
	 */
	void MyTextEdit::wheelEvent(QWheelEvent * event)
	{
		// ignore event and send it up in the event hierarchy
		event->ignore();
		emit wheelMove( event );
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-12-15
	 * \date 2006-01-30 (update)
	 *
	 * \brief Handles key event, check if command completion or eval, 
	 * otherwise send them to the textbrowser
	 *
	 * 2006-01-30 AF, added ignore to 'Alt+Enter'
	 */
	void MyTextEdit::keyPressEvent(QKeyEvent *event )
	{
		// EVAL, key: SHIFT + RETURN || SHIFT + ENTER
		if( event->modifiers() == Qt::ShiftModifier && 
			(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) )
		{
			inCommand = false;
			stopHighlighter = false;

			event->accept();
			emit eval();
		}
		// COMMAND COMPLETION, key: SHIFT + TAB (= BACKTAB) || CTRL + SPACE
		else if( (event->modifiers() == Qt::ShiftModifier && event->key() == Qt::Key_Backtab ) || 
			(event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Space) )
		{
			stopHighlighter = false;

			event->accept();
			if( inCommand )
			{
				emit nextCommand();
			}
			else
			{
				inCommand = true;
				emit command();
			}
		}
		// COMMAND COMPLETION- NEXT FIELD, key: CTRL + TAB
		else if( event->modifiers() == Qt::ControlModifier &&
			event->key() == Qt::Key_Tab )
		{
			stopHighlighter = false;

			event->accept();
			inCommand = false;
			emit nextField();
		}
		// BACKSPACE, DELETE
		else if( event->key() == Qt::Key_Backspace ||
			event->key() == Qt::Key_Delete )
		{
			inCommand = false;
			stopHighlighter = true;
			
			QTextBrowser::keyPressEvent( event );
		}
		// ALT+ENTER (ignore)
		else if( event->modifiers() == Qt::AltModifier &&
			( event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ))
		{
			inCommand = false;
			stopHighlighter = false;

			event->ignore();
		}
		else
		{
			inCommand = false;
			stopHighlighter = false;

			QTextBrowser::keyPressEvent( event );
		}
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2006-01-23
	 *
	 * \brief If the mimedata that should be insertet contain text,
	 * create a new mimedata object that only contains text, otherwise
	 * text format is insertet also - don't want that for inputcells.
	 */
	void MyTextEdit::insertFromMimeData(const QMimeData *source)
	{
		if( source->hasText() )
		{
			QMimeData *newSource = new QMimeData();
			newSource->setText( source->text() );
			QTextBrowser::insertFromMimeData( newSource );
			delete newSource;
		}
		else
			QTextBrowser::insertFromMimeData( source );
	}

	/*! 
	* \class InputCell
	* \author Ingemar Axelsson and Anders Fernstr�m
	*
	* \brief Describes how an inputcell works.
	*
	* Input cells is places where the user can do input. To evaluate
	* the content of an inputcell just press shift+enter. It will
	* throw an exception if it cant find OMC. Start OMC with
	* following commandline: 
	*
	* # omc +d=interactiveCorba
	*
	*
	* \todo Make it possiblee to add and change syntax coloring of code.(Ingemar Axelsson)
	*/

	int InputCell::numEvals_ = 1;

	/*! 
	 * \author Ingemar Axelsson and Anders Fernstr�m
	 * \date 2005-11-23 (update)
	 *
	 * \brief The class constructor
	 *
	 * 2005-10-27 AF, updated the method due to porting from Q3Support 
	 * to pure QT4 classes.
	 * 2005-11-23 AF, added document to the constructor, because need 
	 * the document to insert images to the output part if ploting.
	 */
	InputCell::InputCell(Document *doc, QWidget *parent)
		: Cell(parent), 
		evaluated_(false), 
		closed_(true), 
		delegate_(0),
		document_(doc)
	{
		QWidget *main = new QWidget(this);
		setMainWidget(main);
		
		layout_ = new QGridLayout(mainWidget());
		layout_->setMargin(0);
		layout_->setSpacing(0);

		setTreeWidget(new InputTreeView(this));
		setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));

		//2005-10-07 AF, Porting, change from 'QWidget::' to 'Qt::'
		setFocusPolicy(Qt::NoFocus);

		createInputCell();
		createOutputCell();

		setBackgroundColor(QColor(200,200,255));  
	}

	/*! 
	 * \author Ingemar Axelsson and Anders Fernstr�m
	 *
	 * \brief The class destructor
	 */
	InputCell::~InputCell()
	{
		//2006-01-05 AF, check if input texteditor is in the highlighter,
		//if it is - wait for 60 ms and check again.
		HighlighterThread *thread = HighlighterThread::instance();
		int sleepTime = 0;
		bool firstTime = true;
		while( thread->haveEditor( input_ ) )
		{
			if( firstTime )
			{
				thread->removeEditor( input_ );
				firstTime = false;
			}

			SleeperThread::msleep( 60 );
			sleepTime++;

			if( sleepTime > 100 )
				break;
		}


		delete input_;
		delete output_;
		//delete syntaxHighlighter_;
	}

	/*! 
	 * \author Anders Fernstr�m and Ingemar Axelsson
	 * \date 2005-12-15 (update)
	 *
	 * \brief Creates the QTextEdit for the input part of the 
	 * inputcell
	 *
	 * 2005-10-27 AF, Large part of this function was changes due to 
	 * porting to QT4 (changes from Q3TextEdit to QTextEdit).
	 * 2005-12-15 AF, Added more connections to the editor, mostly for
	 * commandcompletion, but also for eval. invoking eval have moved
	 * from the eventfilter on this cell to the reimplemented key event
	 * handler in the editor
	 */
	void InputCell::createInputCell()
	{
		input_ = new MyTextEdit( mainWidget() );
		layout_->addWidget( input_, 1, 1 );

		//input_->setReadOnly( false );
		input_->setReadOnly( true );
		input_->setUndoRedoEnabled( true );
		//input_->setFrameStyle( QFrame::NoFrame );
		input_->setFrameShape( QFrame::Panel );
		input_->setAutoFormatting( QTextEdit::AutoNone );

		input_->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		input_->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		input_->setContextMenuPolicy( Qt::NoContextMenu );

		QPalette palette;
		palette.setColor(input_->backgroundRole(), QColor(200,200,255));
		input_->setPalette(palette);

		// is this needed, don't know /AF
		input_->installEventFilter(this);
		

		connect( input_, SIGNAL( textChanged() ),
			this, SLOT( contentChanged() ));
		connect( input_, SIGNAL( clickOnCell() ),
			this, SLOT( clickEvent() ));
		connect( input_, SIGNAL( wheelMove(QWheelEvent*) ),
			this, SLOT( wheelEvent(QWheelEvent*) ));
		// 2005-12-15 AF, new connections
		connect( input_, SIGNAL( eval() ),
			this, SLOT( eval() ));
		connect( input_, SIGNAL( command() ),
			this, SLOT( command() ));
		connect( input_, SIGNAL( nextCommand() ),
			this, SLOT( nextCommand() ));
		connect( input_, SIGNAL( nextField() ),
			this, SLOT( nextField() ));
		//2005-12-29 AF
		connect( input_, SIGNAL( textChanged() ),
			this, SLOT( addToHighlighter() ));
		// 2006-01-17 AF, new...
		connect( input_, SIGNAL( currentCharFormatChanged(const QTextCharFormat &) ),
			this, SLOT( charFormatChanged(const QTextCharFormat &) ));

		contentChanged();
	}

	/*! 
	 * \author Anders Fernstr�m and Ingemar Axelsson
	 * \date 2005-10-28 (update)
	 *
	 * \brief Creates the QTextEdit for the output part of the 
	 * inputcell
	 *
	 * Large part of this function was changes due to porting
	 * to QT4 (changes from Q3TextEdit to QTextEdit).
	 */
	void InputCell::createOutputCell()
	{
		output_ = new MyTextEdit( mainWidget() );
		layout_->addWidget( output_, 2, 1 );

		output_->setReadOnly( true );
		output_->setFrameShape( QFrame::Panel );
		output_->setAutoFormatting( QTextEdit::AutoNone );

		output_->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		output_->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		output_->setContextMenuPolicy( Qt::NoContextMenu );

		connect( output_, SIGNAL( textChanged() ),
			this, SLOT(contentChanged()));
		connect( output_, SIGNAL( clickOnCell() ),
			this, SLOT( clickEvent() ));
		connect( output_, SIGNAL( wheelMove(QWheelEvent*) ),
			this, SLOT( wheelEvent(QWheelEvent*) ));

		// Set the correct style for the QTextEdit output_
		Stylesheet *sheet = Stylesheet::instance( "stylesheet.xml" );
		CellStyle style = sheet->getStyle( "Output" );

		if( style.name() != "null" )
		{
			output_->setAlignment( (Qt::AlignmentFlag)style.alignment() );
			output_->mergeCurrentCharFormat( (*style.textCharFormat()) );
			output_->document()->rootFrame()->setFrameFormat( (*style.textFrameFormat()) );
		}
		else
		{
			// 2006-01-30 AF, add message box
			QString msg = "No Output style defened, please define a Output style in stylesheet.xml";
			QMessageBox::warning( 0, "Warning", msg, "OK" );
		}

		output_->hide();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-10-27
	 *
	 * \brief Returns the text (as plain text) fromthe cell
	 *
	 * \return The text, as plain text
	 */
	QString InputCell::text()
	{
		return input_->toPlainText();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-10-27
	 *
	 * \brief Return the text inside the cell as Html code
	 *
	 * \return Html code
	 */
	QString InputCell::textHtml()
	{
		return input_->toHtml();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-23
	 *
	 * \brief Return the text inside the output part of the cell
	 * as plain text
	 *
	 * \return output text
	 */
	QString InputCell::textOutput()
	{
		return output_->toPlainText();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-23
	 *
	 * \brief Return the text inside the output part of the cell
	 * as html code
	 *
	 * \return html code
	 */
	QString InputCell::textOutputHtml()
	{
		return output_->toHtml();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-10-27
	 *
	 * \brief Return the text cursor to the QTextEdit that make up
	 * the inputpart of the inputcell
	 *
	 * \return Text cursor to the cell
	 */
	QTextCursor InputCell::textCursor()
	{
		return input_->textCursor();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2006-01-05
	 *
	 * \brief Return the input texteditor
	 *
	 * \return Texteditor for the inputpart of the inputcell
	 */
	QTextEdit *InputCell::textEdit()
	{
		return input_;
	}

	/*! 
	 * \author Ingemar Axelsson and Anders Fernstr�m
	 * \date 2005-12-16 (update)
	 *
	 * \brief Set text to the cell
	 *
	 * \param text The text that should be placed inside the cell
	 *
	 * 2005-10-04 AF, added some code for removing/replacing some text
	 * 2005-10-27 AF, updated the function due to porting from qt3 to qt4
	 * 2005-12-08 AF, added code that removed any <span style tags added 
	 * in the parser.
	 * 2005-12-16 AF, block signlas so syntax highligher isn't done more 
	 * than once.
	 */
	void InputCell::setText(QString text)
	{
		// 2005-12-16 AF, block signals
		input_->document()->blockSignals(true);
	
		// 2005-10-04 AF, added som code to replace/remove
		QString tmp = text.replace("<br>", "\n");
		tmp.remove( "&nbsp;" );

		// 2005-12-08 AF, remove any <span style tag
		QRegExp spanEnd( "</span>" );
		tmp.remove( spanEnd );
		int pos = 0;
		while( true )
		{
			int startpos = tmp.indexOf( "<span", pos, Qt::CaseInsensitive );
			if( startpos >= 0 )
			{
				int endpos = tmp.indexOf( "\">", startpos );
				if( endpos >= 0 )
				{
					endpos += 2;
					tmp.remove( startpos, endpos - startpos );
				}
				else
					break;
			}
			else
				break;
			
			pos = startpos;
		}

		// set the text
		input_->setPlainText( tmp );

		// 2005-12-16 AF, unblock signals and tell highlighter to highlight
		input_->document()->blockSignals(false);
		input_->document()->setHtml( input_->toHtml() );
		input_->document()->rootFrame()->setFrameFormat( (*style().textFrameFormat()) );

		contentChanged();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-01
	 *
	 * \brief Sets the visible text using html code.
	 *
	 * Sets the text that should be visible using html code. Can change 
	 * the cellheight if the text is very long.
	 *
	 * \param html Html code that should be visible as normal text inside the cell mainarea.
	 */ 
	void InputCell::setTextHtml(QString html)
	{
		input_->setHtml( html );
		setStyle( style() );

		contentChanged();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-23
	 *
	 * \brief Set text to the output part of the cell
	 *
	 * \param text The text that should be placed inside the output part
	 */
	void InputCell::setTextOutput(QString text)
	{
		if( !text.isNull() && !text.isEmpty() )
		{
			output_->setPlainText( text );
			evaluated_ = true;
			//setClosed( false );

			contentChanged();
		}
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-23
	 *
	 * \brief Sets the output text using html code.
	 *
	 * Sets the text that should be visible in the output part of the
	 * cell using html code. Can change the cellheight if the text is 
	 * very long.
	 *
	 * \param html Html code that should be visible as normal text inside the cell mainarea.
	 */ 
	void InputCell::setTextOutputHtml(QString html)
	{
		if( !html.isNull() && !html.isEmpty() )
		{
			output_->setHtml( html );
			evaluated_ = true;
			//setClosed( false );

			contentChanged();
		}
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-10-28
	 *
	 * \brief Set cell style
	 *
	 * IMPORTANT: User shouldn't be able to change style on inputcells 
	 * so this function always use "Input" as style.
	 *
	 * \param stylename The style name of the style that is to be applyed to the cell
	 */
	void InputCell::setStyle(const QString &)
	{
		Cell::setStyle( "Input" );
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-10-27
	 * \date 2005-11-03 (update)
	 *
	 * \brief Set cell style
	 *
	 * IMPORTANT: User shouldn't be able to change style on inputcells 
	 * so this function always use "Input" as style.
	 *
	 * 2005-11-03 AF, updated so the text is selected when the style
	 * is changed, after the text is unselected.
	 *
	 * \param style The cell style that is to be applyed to the cell
	 */
	void InputCell::setStyle(CellStyle style)
	{
		if( style.name() == "Input" )
		{
			Cell::setStyle( style );

			// select all the text
			input_->selectAll();
		
			// set the new style settings
			input_->setAlignment( (Qt::AlignmentFlag)style_.alignment() );
			input_->mergeCurrentCharFormat( (*style_.textCharFormat()) );
			input_->document()->rootFrame()->setFrameFormat( (*style_.textFrameFormat()) );

			// unselect the text
			QTextCursor cursor(	input_->textCursor() );
			cursor.clearSelection();
			input_->setTextCursor( cursor );
		}
		else
		{
			setStyle( "Input" );
		}
	}

	/*!
	 * \author Anders Fernstr�m
	 * \date 2005-11-01
	 *
	 * \breif Set readonly value on the texteditor
	 *
	 * \param readonly The boolean value of readonly property
	 */
	void InputCell::setReadOnly(const bool readonly)
	{
		if( readonly )
		{
			QTextCursor cursor1 = input_->textCursor();
			QTextCursor cursor2 = output_->textCursor();
			cursor1.clearSelection();
			cursor2.clearSelection();
			input_->setTextCursor( cursor1 );
			output_->setTextCursor( cursor2 );
		}

		input_->setReadOnly(readonly);
	}

	/*!
	 * \author Anders Fernstr�m
	 * \date 2006-01-16
	 *
	 * \breif Set evaluated value on the texteditor
	 *
	 * \param evaluated The boolean value of evaluated property
	 */
	void InputCell::setEvaluated(const bool evaluated)
	{
		evaluated_ = evaluated;
	}

	/*!
	 * \author Ingemar Axelsson (and Anders Fernstr�m)
	 * \date 2005-11-01 (update)
	 *
	 * \breif Set if the output part of the cell shoud be 
	 * closed(hidden) or not.
	 *
	 * 2005-11-01 AF, Made some small changes to how the function 
	 * calculate the new height, to reflect the changes made when 
	 * porting from Q3TextEdit to QTextEdit. 
	 */
	void InputCell::setClosed(const bool closed)
	{
		if( closed )
			output_->hide();
		else
		{
			if( evaluated_ )
				output_->show();
		}

		closed_ = closed;
		contentChanged();
	}

	/*!
	 * \author Ingemar Axelsson and Anders Fernstr�m
	 */
	void InputCell::clickEvent()
	{
		//if( input_->isReadOnly() )
			emit clicked(this);
	}

	/*!
	 * \author Anders Fernstr�m and Ingemar Axelsson
	 * \date 2005-10-31 (update)
	 *
	 * \breif Recalculates height. 
	 *
	 * Large part of this function was changes due to porting
	 * to QT4 (changes from Q3TextBrowser to QTextBrowser). /AF
	 */
	void InputCell::contentChanged()
	{
		int height = input_->document()->documentLayout()->documentSize().toSize().height();

		if( height < 0 )
			height = 30;

		// add a little extra, just in case /AF
		input_->setMinimumHeight( height + 5 );

		if( evaluated_ && !closed_ )
		{	
			int outHeight = output_->document()->documentLayout()->documentSize().toSize().height();

			if( outHeight < 0 )
				outHeight = 30;

			output_->setMinimumHeight( outHeight );
			height += outHeight;
		}

		setHeight( height );
		emit textChanged();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2006-01-17
	 *
	 * \brief Returns true if inputcell is closed, otherwise the method
	 * returns false.
	 *
	 * \return State of inputcell (closed or not)
	 */
	bool InputCell::isClosed()
	{
		return closed_;
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-10-27
	 *
	 * \brief Function for telling if the user is allowed to change 
	 * the text settings for the text inside the cell. User isn't
	 * allowed to change the text settings for inputcell so this 
	 * function always return false.
	 *
	 * \return False
	 */
	bool InputCell::isEditable()
	{
		return false;
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-23
	 *
	 * \brief Returns true if inputcell is evaluated, returns false if
	 * inputcell haven't been evaluated.
	 *
	 * \return State of inputcell (evaluated or not)
	 */
	bool InputCell::isEvaluated()
	{
		return evaluated_;	
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-11-23
	 *
	 * \brief Returns true if the expression in the text is a plot 
	 * command, returns false otherwise. If no text is sent to the 
	 * method it will test the text in the input part of the cell.
	 *
	 * \param text The text that should be tested, if no text the 
	 * inputpart of the cell will be tested.
	 * \return If plot command or not
	 */
	bool InputCell::isPlot(QString text)
	{
		QRegExp exp( "plot(.*)|plotParametric(.*)" );

		if( text.isNull() )
		{
			if( 0 <= input_->toPlainText().indexOf( exp, 0 ) )
				return true;
			else
				return false;
		}
		else
		{
			if( 0 <= text.indexOf( exp, 0 ) )
				return true;
			else
				return false;
		
		}
	}

	/*! 
	 * \author Ingemar Axelsson and Anders Fernstr�m
	 * \date 2005-11-23 (update)
	 *
	 *\brief Sends the content of the inputcell to the evaluator. 
	 * Displays the result in a outputcell. 
	 *
	 * 2005-11-01 AF, updated so the text that is sent to be evaled isn't
	 * in html code.
	 * 2005-11-17 AF, added a check if the result if empty, if so add
	 * some default text
	 * 2005-11-23 AF, added support for inserting image to output
	 *
	 * Removes whitespaces and tags from the content string. Then sends
	 * the content to the delegate object for evaluation. The result is
	 * printed in a output cell. No indentation and syntax
	 * highlightning is used in the output cell.
	 * 
	 */
	void InputCell::eval()
	{
		input_->blockSignals(true);
		output_->blockSignals(true);

		// Only the text, no html tags. /AF
		QString expr = input_->toPlainText();
		expr = expr.simplified();

		if(hasDelegate())
		{
			QDir dir;
			QString imagename = "omc_tmp_plot.png";

			// remove plot.png if it already exist, don't want any
			// old plot.
			if( isPlot() )
			{
				if( dir.exists( imagename ))
					dir.remove( imagename );
			}

			delegate()->evalExpression( expr );

			// 2005-11-24 AF, added check to see if the user wants to quit
			if( 0 == expr.indexOf( "quit()", 0, Qt::CaseInsensitive ))
			{
				qApp->closeAllWindows();
			}

			// get the result
			QString res = delegate()->getResult();
			QString error = delegate()->getError();

			// if the expression is a plot command and the is no errors
			// in the result, find the image and insert it into the 
			// output part of the cell.
			if( isPlot() && error.isEmpty() )
			{	
				output_->setPlainText( "creating plot..." );
				output_->update();

				int sleepTime = 1;
				while( true )
				{
					if( dir.exists( imagename ))
					{
						QImage *image = new QImage( imagename );
						if( !image->isNull() )
						{
							QString newname = document_->addImage( image );
							QTextCharFormat format = output_->currentCharFormat();
																				
							QTextImageFormat imageformat;
							imageformat.merge( format );
							imageformat.setHeight( image->height() );
							imageformat.setWidth( image->width() );
							imageformat.setName( newname );

							output_->setPlainText("");
							output_->textCursor().insertImage( imageformat );
						}
						else
							output_->setPlainText( "[Error] Unable to read plot image \"" + imagename + "\"" );

						break;
					}

					if( sleepTime > 25 )
					{
						output_->setPlainText( "[Error] Unable to found plot image \"" + imagename + "\"" );
						break;
					}
					
					SleeperThread::msleep( 1000 );
					sleepTime++;
				}

			}
			else
			{
				// check if resualt is empty
				if( res.isEmpty() )
					res = "[done]";

				if( !error.isEmpty() )
					res += QString("\n") + error;
			
				output_->setPlainText( res );
			}

			++numEvals_;
			evaluated_ = true;
			setClosed(false);
			dir.remove( imagename );
		}

		input_->blockSignals(false);
		output_->blockSignals(false);
		contentChanged();
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-12-15
	 *
	 *\brief Get/Insert the command that match the last word in the 
	 * input editor.
	 */
	void InputCell::command()
	{
		qDebug("Command");
		CommandCompletion *commandcompletion = CommandCompletion::instance( "commands.xml" );
		QTextCursor cursor = input_->textCursor();
		
		if( commandcompletion->insertCommand( cursor ))
			input_->setTextCursor( cursor );
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-12-15
	 *
	 *\brief Get/Insert the next command that match the last word in 
	 * the input editor.
	 */
	void InputCell::nextCommand()
	{
		qDebug("Next Command");
		CommandCompletion *commandcompletion = CommandCompletion::instance( "commands.xml" );
		QTextCursor cursor = input_->textCursor();
		
		if( commandcompletion->nextCommand( cursor ))
			input_->setTextCursor( cursor );
	}

	/*! 
	 * \author Anders Fernstr�m
	 * \date 2005-12-15
	 *
	 *\brief Select the next field in the command, if any exists
	 */
	void InputCell::nextField()
	{
		qDebug("Next Field");
		CommandCompletion *commandcompletion = CommandCompletion::instance( "commands.xml" );
		QTextCursor cursor = input_->textCursor();
		
		if( commandcompletion->nextField( cursor ))
			input_->setTextCursor( cursor );
	}

	/*!
	 * \author Anders Fernstr�m
	 * \date 2005-12-29
	 * \date 2006-01-16 (update)
	 *
	 * \breif adds the input text editor to the highlighter thread
	 * when text have changed.
	 *
	 * 2006-01-16 AF, don't add text editor if MyTextEdit says NO
	 */
	void InputCell::addToHighlighter()
	{
		emit textChanged(true);

		if( input_->toPlainText().isEmpty() )
			return;

		// 2006-01-16 AF, Don't add the text editor if mytextedit 
		// don't allow it. mytextedit says no if the user removes
		// text (backspace or delete).
		if( dynamic_cast<MyTextEdit *>(input_)->isStopingHighlighter() )
			return;
		
		HighlighterThread *thread = HighlighterThread::instance();
		thread->addEditor( input_ );
	}

	/*!
	 * \author Anders Fernstr�m
	 * \date 2006-01-17
	 *
	 * \breif set the correct style if the charFormat is changed and the
	 * cell is empty. This is done because otherwise the style is lost if
	 * all text is removed inside a cell.
	 */
	void InputCell::charFormatChanged(const QTextCharFormat &)
	{
		if( input_->toPlainText().isEmpty() )
		{
			input_->blockSignals( true );
			input_->setAlignment( (Qt::AlignmentFlag)style().alignment() );
			input_->mergeCurrentCharFormat( (*style().textCharFormat()) );
			input_->document()->rootFrame()->setFrameFormat( (*style().textFrameFormat()) );
			input_->blockSignals( false );
			contentChanged();
		}
	}



	// ***************************************************************


	/*! \brief Sets the evaulator delegate.
	*/
	void InputCell::setDelegate(InputCellDelegate *d)
	{
		delegate_ = d;
	}

	InputCellDelegate *InputCell::delegate()
	{
		if(!hasDelegate())
			throw exception("No delegate.");

		return delegate_;
	}



	bool InputCell::hasDelegate()
	{
		return delegate_ != 0;
	}








	

	/*! \brief Do not use this member.
	*
	* This is an ugly part of the cell structure.
	*/
	void InputCell::addCellWidgets()
	{
		layout_->addWidget(input_,0,0);	 

		if(evaluated_)
			layout_->addWidget(output_,1,0);
	}

	void InputCell::removeCellWidgets()
	{
		/*
		// PORT >> layout_->remove(input_);
		if(evaluated_)
			layout_->remove(output_);
			*/

		layout_->removeWidget(input_);
		if(evaluated_)
			layout_->removeWidget(output_);
	}

	/*! \brief resets the input cell. Removes all output data and
	*  restores the initial state.
	*/
	void InputCell::clear()
	{
		if(evaluated_)
		{
			output_->clear();
			evaluated_ = false;
			// PORT >> layout_->remove(output_);
			layout_->removeWidget(output_);
		}

		//input_->setReadOnly(false);
		input_->setReadOnly(true);
		input_->clear();
		treeView()->setClosed(false); //Notis this
		setClosed(true);
	}

	/*!
	* Resize textcell when the mainwindow is resized. This because the
	* cellcontent should always be visible.
	*
	* Added by AF, copied from textcell.cpp
	*/
	void InputCell::resizeEvent(QResizeEvent *event)
	{
		contentChanged(); 
		Cell::resizeEvent(event);
	}




	void InputCell::setFocus(const bool focus)
	{
		if(focus)
			input_->setFocus();
	}

	void InputCell::mouseDoubleClickEvent(QMouseEvent *)
	{
		// PORT >>if(treeView()->hasMouse())
		if(treeView()->testAttribute(Qt::WA_UnderMouse))
		{
			setClosed(!closed_);
		}
	}

	void InputCell::accept(Visitor &v)
	{
		v.visitInputCellNodeBefore(this);

		if(hasChilds())
			child()->accept(v);

		v.visitInputCellNodeAfter(this);

		if(hasNext())
			next()->accept(v);
	}

}