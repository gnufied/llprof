package RRProf;

import java.awt.BasicStroke;
import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GridLayout;
import java.awt.Stroke;
import java.util.Random;

import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JSlider;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;

import RRProf.DataStore.Record;

public class SquaresView extends JPanel implements ChangeListener {

	MonitorBrowser browser;
	Stroke boxStroke;
	CanvasPanel canvas;
	JPanel ctrlpanel;
	JSlider max_depth_slider;

	SquaresView(MonitorBrowser b) {
		browser = b;
		setBackground(Color.white);
		boxStroke = new BasicStroke(1.0f);

		setLayout(new BorderLayout());
		
		canvas = new CanvasPanel();
		ctrlpanel = new JPanel();
		ctrlpanel.setLayout(new GridLayout(10, 0));
		ctrlpanel.setBounds(0,0,100,100);
		add(ctrlpanel, BorderLayout.WEST);
		add(canvas, BorderLayout.CENTER);
		
		ctrlpanel.add(new JLabel("Max depth"));
		max_depth_slider = new JSlider(1, 40);
		max_depth_slider.setOrientation(JSlider.HORIZONTAL);
		max_depth_slider.setValue(5);
		max_depth_slider.addChangeListener(this);
		ctrlpanel.add(max_depth_slider);
	}

	private static final int ORIENT_V = 1;
	private static final int ORIENT_H = 2;

	
	class PaintContext {
		Graphics2D g;
		Record r;
		int x,y,w,h;
		int depth;
		int orient;
		Random rand;
	}
	
	void drawBox(PaintContext ctx) {
		ctx.rand.setSeed(ctx.r.getID());
		ctx.rand.setSeed(ctx.rand.nextLong());
		float cr = ctx.rand.nextFloat() * 0.3f + 0.7f;
		float cg = ctx.rand.nextFloat() * 0.3f + 0.7f;
		float cb = ctx.rand.nextFloat() * 0.3f + 0.7f;
		ctx.g.setColor(new Color(cr,cg,cb));
		ctx.g.fillRect(ctx.x, ctx.y, ctx.w, ctx.h);
		
		ctx.g.setColor(Color.black);
		ctx.g.setStroke(boxStroke);
		ctx.g.drawRect(ctx.x, ctx.y, ctx.w, ctx.h);
	}

	void paintRecord(PaintContext ctx) {
		if(ctx.depth <= 0)
			return;
		drawBox(ctx);

		ctx.g.setColor(Color.black);
		String label = ctx.r.getTargetName();
		if(label == null)
			label = "<null>";
		ctx.g.drawString(label, ctx.x + ctx.w/2, ctx.y + ctx.h/2);

		double alltime = 0.0;
		double prev_pos = 0.0;
		if(ctx.r.getAllTime() == 0) {
			for(int i = 0; i < ctx.r.getChildCount(); i++) {
				alltime += ctx.r.getChild(i).getAllTime();
			}
		}else {
			alltime = ctx.r.getAllTime();
		}
		
		PaintContext child_ctx = new PaintContext();
		child_ctx.depth = ctx.depth - 1;
		child_ctx.rand = ctx.rand;
		child_ctx.g = ctx.g;
		child_ctx.r = ctx.r;
		child_ctx.orient = ctx.orient == ORIENT_H ? ORIENT_V : ORIENT_H;
		for(int i = 0; i < ctx.r.getChildCount(); i++)
		{
			child_ctx.r = ctx.r.getChild(i);
			double p = (double)(child_ctx.r.getAllTime()) / alltime;
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
			paintRecord(child_ctx);
			prev_pos += p;
		}
	}
	
	class CanvasPanel extends JPanel {

		public void paintComponent(Graphics g_orig) {
			Graphics2D g = (Graphics2D)g_orig;
			paintTo(g);
		}
	}
	
	public void paintTo(Graphics2D g) {
		g.setBackground(Color.white);
		g.clearRect(0,0,this.getWidth(), this.getHeight());
		
		
		Record target_record = null;
		try{
			target_record = browser.getMonitor().getDataStore().getRootRecord(0);
		} catch(NullPointerException e) {
			return;
		}
		PaintContext ctx = new PaintContext();
		ctx.rand = new Random(0);
		ctx.g = g;
		ctx.r = target_record;
		ctx.x = 4;
		ctx.y = 4;
		ctx.orient = ORIENT_V;
		ctx.w = canvas.getWidth()-8;
		ctx.h = canvas.getHeight()-8;
		ctx.depth = max_depth_slider.getValue();
		paintRecord(ctx);
	}

	@Override
	public void stateChanged(ChangeEvent e) {
		
		
	}
}
