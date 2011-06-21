package RRProf;

import java.util.*;
import java.util.Timer;
import java.io.*;
import java.awt.*;
import java.awt.event.*;

import javax.swing.*;

public class MonitorGUI extends JFrame implements ActionListener, MouseListener {

	class BrowserCellRenderer extends JLabel implements ListCellRenderer {
		/*
		final static ImageIcon longIcon = new ImageIcon("long.gif");
		final static ImageIcon shortIcon = new ImageIcon("short.gif");
		*/
		// This is the only method defined by ListCellRenderer.
		// We just reconfigure the JLabel each time we're called.

		public Component getListCellRendererComponent(JList list, // the list
				Object value, // value to display
				int index, // cell index
				boolean isSelected, // is the cell selected
				boolean cellHasFocus) // does the cell have focus
		{
			MonitorBrowser b = (MonitorBrowser)value;
			String label = b.getListLabel();
			
			setText(label);
			// setIcon((s.length() > 10) ? longIcon : shortIcon);
			if (isSelected) {
				setBackground(list.getSelectionBackground());
				setForeground(list.getSelectionForeground());
			} else {
				setBackground(list.getBackground());
				setForeground(list.getForeground());
			}
			setEnabled(list.isEnabled());
			setFont(list.getFont());
			setOpaque(true);
			return this;
		}
	}

	JList browserList;
	DefaultListModel browserListModel;
	MonitorBrowser currentBrowser;
	JSplitPane rootPane;
	JPopupMenu browserListPopup;
	JMenuBar menuBar;
	
	public static final String WINDOW_TITLE = "Profiler Monitor";

	Timer timer;
	MonitorGUI() {
		currentBrowser = null;
		setTitle(WINDOW_TITLE);
		setBounds(60, 60, 800, 900);
		setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

		menuBar = new JMenuBar();
		JMenu menuFile = new JMenu("File");
		menuFile.setMnemonic('F');
		menuBar.add(menuFile);
		JMenuItem menuConnect = new JMenuItem("Connect");
		menuConnect.setMnemonic('C');
		menuConnect.addActionListener(this);
		menuFile.add(menuConnect);
		JMenuItem menuOpenActive = new JMenuItem("Open Active");
		menuOpenActive.setMnemonic('O');
		menuOpenActive.addActionListener(this);
		menuFile.add(menuOpenActive);
		JMenuItem menuOpenHeavy = new JMenuItem("Open Heavy");
		menuOpenHeavy.setMnemonic('H');
		menuOpenHeavy.addActionListener(this);
		menuFile.add(menuOpenHeavy);
		JMenuItem menuExportXML = new JMenuItem("Export XML");
		menuExportXML.setMnemonic('X');
		menuExportXML.addActionListener(this);
		menuFile.add(menuExportXML);
		getRootPane().setJMenuBar(menuBar);

		rootPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT);

		browserList = new JList();
		browserListModel = new DefaultListModel();
		browserList.setCellRenderer(new BrowserCellRenderer());
		browserList.setModel(browserListModel);
		rootPane.setDividerSize(5);
		rootPane.setLeftComponent(browserList);
		rootPane.setRightComponent(new JPanel());
		rootPane.setDividerLocation(240);

		getContentPane().add(rootPane);
		
		
		// List popup menu
		browserListPopup = new JPopupMenu();
		browserList.addMouseListener(this);
		
		JMenuItem item;
		item = new JMenuItem("Disconnect");
		item.addActionListener(this);
		item.setActionCommand("browser.disconnect");
		browserListPopup.add(item);

		item = new JMenuItem("Reconnect");
		item.addActionListener(this);
		item.setActionCommand("browser.reconnect");
		browserListPopup.add(item);

		timer = new Timer();
		timer.schedule(new ProfileTimer(), 0, 500);
	}

	
	public void mousePressed(MouseEvent e) {
		if (e.getComponent() == browserList && e.isPopupTrigger()) {
			browserListPopup.show(e.getComponent(), e.getX(), e.getY());
		}
	}
	public void mouseReleased(MouseEvent e) {
		if (e.getComponent() == browserList && e.isPopupTrigger()) {
			browserListPopup.show(e.getComponent(), e.getX(), e.getY());
		}
	}
	public void mouseClicked(MouseEvent e) {
	}
	public void mouseEntered(MouseEvent e) {
	}
	public void mouseExited(MouseEvent e) {
	}
	
	public MonitorBrowser newBrowser() {
		MonitorBrowser b = new MonitorBrowser();
		browserListModel.addElement(b);
		browserList.repaint();
		return b;
	}

	public void setActiveBrowser(MonitorBrowser b) {
		if(currentBrowser != b) {
			setTitle(WINDOW_TITLE + " - " + b.getListLabel());
			rootPane.setRightComponent(b);
			rootPane.repaint();
			currentBrowser = b;
		}
	}
	
	public void actionPerformed(ActionEvent e) {


		if (e.getActionCommand() == "Connect") {
			ConnectDialog dlg = new ConnectDialog(this);
			dlg.setVisible(true);
		} else if (e.getActionCommand() == "Open Active") {
			currentBrowser.callTreeBrowser
					.openTree(CallTreeBrowserView.OTM_ACTIVE);
		} else if (e.getActionCommand() == "Open Heavy") {
			currentBrowser.callTreeBrowser
					.openTree(CallTreeBrowserView.OTM_BOTTLENECK);
		} else if (e.getActionCommand() == "Export XML") {
			JFileChooser filechooser = new JFileChooser();

			int selected = filechooser.showSaveDialog(this);
			if (selected == JFileChooser.APPROVE_OPTION) {
				File file = filechooser.getSelectedFile();
				currentBrowser.getMonitor().getDataStore().exportXML(file.getName());
			} else if (selected == JFileChooser.CANCEL_OPTION) {
			} else if (selected == JFileChooser.ERROR_OPTION) {
				System.out.println("Error: Open dialog");
			}

		} else if (e.getActionCommand() == "browser.disconnect") {
			MonitorBrowser b = (MonitorBrowser)browserList.getSelectedValue();
			if(b != null) {
				setActiveBrowser(b);
				b.reset();
			}
		} else if (e.getActionCommand() == "browser.reconnect") {
			MonitorBrowser b = (MonitorBrowser)browserList.getSelectedValue();
			if(b != null) {
				setActiveBrowser(b);
				b.connect();
			}
		}
		
	}

	public void connect(String host, int port, int interval) {
		MonitorBrowser b = newBrowser();
		b.connect(host, port, interval);
		setActiveBrowser(b);
		browserList.repaint();
	}

	class ProfileTimer extends TimerTask {
		public void run() {
			if(currentBrowser != null)
				currentBrowser.updateProfiler();
		}
	}

	public static void main(String[] args) throws Exception {
		MonitorGUI frame = new MonitorGUI();
		frame.setVisible(true);

		if (args.length > 2) {
			frame.connect(args[0], Integer.parseInt(args[1]), Integer
					.parseInt(args[2]));
		}

	}


}
