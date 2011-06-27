package RRProf;

import java.awt.BasicStroke;
import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GridLayout;
import java.awt.Stroke;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.image.BufferedImage;
import java.util.Random;

import javax.swing.BoxLayout;
import javax.swing.JLabel;
import javax.swing.JMenuItem;
import javax.swing.JPanel;
import javax.swing.JPopupMenu;
import javax.swing.JSlider;
import javax.swing.JTextArea;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;

import RRProf.DataStore.Record;

public class SquaresView extends JPanel implements ChangeListener {

	MonitorBrowser browser;
	Stroke boxStroke;
	CanvasPanel canvas;
	JPanel ctrlpanel;
	JSlider max_depth_slider;
	Record selectedRecord, rootRecord;
	JTextArea selectedLabel, rootLabel, debugLabel;
	int nBoxes;
	SquaresView(MonitorBrowser b) {
		browser = b;
		selectedRecord = null;
		rootRecord = null;
		setBackground(Color.white);
		boxStroke = new BasicStroke(1.0f);

		setLayout(new BorderLayout());
		
		canvas = new CanvasPanel();
		ctrlpanel = new JPanel();
		ctrlpanel.setPreferredSize(new Dimension(200,100));
		ctrlpanel.setLayout(new BoxLayout(ctrlpanel, BoxLayout.Y_AXIS));
		add(ctrlpanel, BorderLayout.WEST);
		add(canvas, BorderLayout.CENTER);

		ctrlpanel.add(new JLabel("Max depth"));
		max_depth_slider = new JSlider(1, 40);
		max_depth_slider.setOrientation(JSlider.HORIZONTAL);
		max_depth_slider.setValue(5);
		max_depth_slider.addChangeListener(this);
		ctrlpanel.add(max_depth_slider);

		ctrlpanel.add(new JLabel("Root:"));
		ctrlpanel.add(rootLabel = new JTextArea("---"));
		rootLabel.setEditable(false);
		rootLabel.setLineWrap(true);
		ctrlpanel.add(new JLabel("Selected:"));
		ctrlpanel.add(selectedLabel = new JTextArea("---"));
		selectedLabel.setEditable(false);
		selectedLabel.setLineWrap(true);

		
		ctrlpanel.add(new JLabel("Debug:"));
		ctrlpanel.add(debugLabel = new JTextArea("---"));

	}

	private static final int ORIENT_V = 1;
	private static final int ORIENT_H = 2;

	
	class PaintContext {
		Graphics2D g;
		Record r;
		int x,y,w,h;
		int mouseX, mouseY;
		int depth;
		int orient;
		Random rand;
		boolean getSelection;
		Record selectedRecord;
	}
	
	void drawBox(PaintContext ctx) {
		if(ctx.w < 1 || ctx.h < 1)
			return;
		nBoxes++;
		ctx.rand.setSeed(ctx.r.getID());
		ctx.rand.setSeed(ctx.rand.nextLong());
		float cr; cr = ctx.rand.nextFloat();
		float cg; cg = ctx.rand.nextFloat();
		float cb; cb = ctx.rand.nextFloat();
		
		if(ctx.selectedRecord == ctx.r)
			ctx.g.setColor(new Color(0.4f, 0.4f, 0.4f));
		else
			ctx.g.setColor(new Color(cr*0.3f+0.7f, cg*0.3f+0.7f, cb*0.3f+0.7f));
		ctx.g.fillRect(ctx.x, ctx.y, ctx.w, ctx.h);
		
		ctx.g.setColor(Color.black);
		ctx.g.setStroke(boxStroke);
		ctx.g.drawRect(ctx.x, ctx.y, ctx.w, ctx.h);
	}

	void paintRecord(PaintContext ctx) {
		if(ctx.depth <= 0)
			return;

		if(ctx.getSelection) {
			// 選択処理
			if(ctx.x < ctx.mouseX && ctx.mouseX < ctx.x + ctx.w && ctx.y < ctx.mouseY && ctx.mouseY < ctx.y + ctx.h) {
				ctx.selectedRecord = ctx.r;
			}
		} else {
			// 描画処理
			drawBox(ctx);
			if(ctx.w > 30 && ctx.h > 12) {
				if(ctx.selectedRecord == ctx.r){
					
				}else{
					ctx.g.setColor(Color.black);
				}
				String label = ctx.r.getTargetName();
				if(label == null)
					label = "<null>";

				FontMetrics fo = ctx.g.getFontMetrics();
				int fw = fo.stringWidth(label);
				int fh = fo.getHeight();
				ctx.g.drawString(label, ctx.x + (ctx.w - fw) / 2, ctx.y + (ctx.h + fh) / 2);
			}
		}

		double alltime = 0.0;
		double prev_pos = 0.0;
		// @todo index 0
		if(ctx.r.getAllValue(0) == 0) {
			for(int i = 0; i < ctx.r.getChildCount(); i++) {
				alltime += ctx.r.getChild(i).getAllValue(0);
			}
		}else {
			alltime = ctx.r.getAllValue(0);
		}
		
		PaintContext child_ctx = new PaintContext();
		child_ctx.getSelection = ctx.getSelection;
		if(ctx.getSelection)
			child_ctx.selectedRecord = null;
		else
			child_ctx.selectedRecord = ctx.selectedRecord;
		child_ctx.mouseX = ctx.mouseX;
		child_ctx.mouseY = ctx.mouseY;
		
		child_ctx.depth = ctx.depth - 1;
		child_ctx.rand = ctx.rand;
		child_ctx.g = ctx.g;
		child_ctx.r = ctx.r;
		child_ctx.orient = ctx.orient == ORIENT_H ? ORIENT_V : ORIENT_H;
		for(int i = 0; i < ctx.r.getChildCount(); i++)
		{
			child_ctx.r = ctx.r.getChild(i);
			/// @todo index zero
			double p = (double)(child_ctx.r.getAllValue(0)) / alltime;
			if(p <= 0.001)
				continue;
			if(ctx.orient == ORIENT_H) {
				child_ctx.x = (int)(ctx.x + prev_pos * ctx.w);
				child_ctx.y = ctx.y;
				child_ctx.w = (int)(ctx.w * p);
				child_ctx.h = ctx.h;
			}
			else {
				child_ctx.x = ctx.x;
				child_ctx.y = (int)(ctx.y + prev_pos * ctx.h);
				child_ctx.w = ctx.w;
				child_ctx.h = (int)(ctx.h * p);
			}
			child_ctx.x += 2;
			child_ctx.y += 2;
			child_ctx.w -= 4;
			child_ctx.h -= 4;
			
			if(ctx.getSelection && child_ctx.selectedRecord != null)
				break;
			
			paintRecord(child_ctx);
			prev_pos += p;
		}
		if(ctx.getSelection && child_ctx.selectedRecord != null)
			ctx.selectedRecord = child_ctx.selectedRecord;
	}
	
	class CanvasPanel extends JPanel implements MouseListener, ActionListener {

		CanvasPanel() {
			addMouseListener(this);
		}
		public void paintComponent(Graphics g) {
			paintTo((Graphics2D)g, selectedRecord);
		}
		@Override
		public void mouseClicked(MouseEvent e) {
			setSelectedRecord(getSelection(e.getX(), e.getY()));
		}
		@Override
		public void mouseEntered(MouseEvent e) {
		}
		@Override
		public void mouseExited(MouseEvent e) {
		}
		@Override
		public void mousePressed(MouseEvent e) {
			if(e.isPopupTrigger()) {
				JPopupMenu popup = new JPopupMenu();
				JMenuItem item;
				item = new JMenuItem("Show only this");
				item.addActionListener(this);
				item.setActionCommand("setroot");
				popup.add(item);

				item = new JMenuItem("Open this in tree");
				item.addActionListener(this);
				item.setActionCommand("opentree");
				popup.add(item);

				item = new JMenuItem("Open this in list");
				item.addActionListener(this);
				item.setActionCommand("openlist");
				popup.add(item);
				
				popup.addSeparator();

				item = new JMenuItem("Back to root");
				item.addActionListener(this);
				item.setActionCommand("backroot");
				popup.add(item);

				item = new JMenuItem("Show parent");
				item.addActionListener(this);
				item.setActionCommand("backparent");
				popup.add(item);
				
				popup.show(e.getComponent(), e.getX(), e.getY());
			}
		}
		@Override
		public void mouseReleased(MouseEvent e) {
		}
		@Override
		public void actionPerformed(ActionEvent e) {
			if(e.getActionCommand() == "setroot") {
				setRootRecord(selectedRecord);
			}
			else if(e.getActionCommand() == "backroot") {
				setRootRecord(null);
			}
			else if(e.getActionCommand() == "backparent") {
				if(rootRecord != null) {
					if(rootRecord.getParent() == null) {
						browser.writeLog("No more parent");
					} else {
						setRootRecord(rootRecord.getParent());
					}
					
				}
			}
			else if(e.getActionCommand() == "opentree") {
				browser.openRecordInTree(selectedRecord);
			}
		}
	}
	
	public void setSelectedRecord(Record rec) {
		selectedRecord = rec;
		paintTo((Graphics2D)canvas.getGraphics(), selectedRecord);
		if(selectedRecord != null)
			selectedLabel.setText(selectedRecord.getPathString(8));
	}
	
	public void setRootRecord(Record rec) {
		rootRecord = rec;
		setSelectedRecord(null);
		if(rootRecord != null)
			rootLabel.setText(rootRecord.getPathString(8));
	}

	BufferedImage backbuf;
	
	public void paintTo(Graphics2D graphics, Record selected) {
		
		
		if(backbuf == null || backbuf.getWidth() != canvas.getWidth() || backbuf.getHeight() != canvas.getHeight()) {
			backbuf = new BufferedImage(canvas.getWidth(), canvas.getHeight(), BufferedImage.TYPE_INT_RGB);
		}
		
		Graphics2D backg = (Graphics2D)backbuf.getGraphics();
		backg.setBackground(Color.white);
		backg.clearRect(0,0,this.getWidth(), this.getHeight());
		
		if(rootRecord == null) {
			try{
				rootRecord = browser.getMonitor().getDataStore().getRootRecord(0);
			} catch(NullPointerException e) {
				return;
			}
		}
		PaintContext ctx = new PaintContext();
		ctx.rand = new Random(0);
		ctx.g = backg;
		ctx.r = rootRecord;
		ctx.x = 4;
		ctx.y = 4;
		ctx.getSelection = false;
		ctx.selectedRecord = selected;
		ctx.orient = ORIENT_V;
		ctx.w = canvas.getWidth()-8;
		ctx.h = canvas.getHeight()-8;
		ctx.depth = max_depth_slider.getValue();
		nBoxes = 0;
		paintRecord(ctx);
		graphics.drawImage(backbuf,0,0,null);
		
		debugLabel.setText("nBoxes: " + Integer.toString(nBoxes));
	}
	
	public Record getSelection(int mx, int my) {
		if(rootRecord == null)
			return null;
		PaintContext ctx = new PaintContext();
		ctx.rand = new Random(0);
		ctx.g = null;
		ctx.r = rootRecord;
		ctx.x = 4;
		ctx.y = 4;
		ctx.getSelection = true;
		ctx.mouseX = mx;
		ctx.mouseY = my;
		ctx.orient = ORIENT_V;
		ctx.w = canvas.getWidth()-8;
		ctx.h = canvas.getHeight()-8;
		ctx.depth = max_depth_slider.getValue();
		paintRecord(ctx);
		return ctx.selectedRecord;
	}

	@Override
	public void stateChanged(ChangeEvent e) {
		
		
	}
}
