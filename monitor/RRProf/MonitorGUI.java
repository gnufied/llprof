package RRProf;

import java.util.*;
import java.util.Timer;
import java.io.*;
import java.net.Socket;
import java.awt.*;
import java.awt.event.*;

import javax.swing.*;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;

public class MonitorGUI extends JFrame implements ActionListener,
		MouseListener, ListSelectionListener {

	class BrowserCellRenderer extends JLabel implements ListCellRenderer {
		/*
		 * final static ImageIcon longIcon = new ImageIcon("long.gif"); final
		 * static ImageIcon shortIcon = new ImageIcon("short.gif");
		 */
		// This is the only method defined by ListCellRenderer.
		// We just reconfigure the JLabel each time we're called.

		public Component getListCellRendererComponent(JList list, // the list
				Object value, // value to display
				int index, // cell index
				boolean isSelected, // is the cell selected
				boolean cellHasFocus) // does the cell have focus
		{
			MonitorBrowser b = (MonitorBrowser) value;
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
	JMenuBar menuBar;
	AggressiveModeServer server;

	public static final String WINDOW_TITLE = "Profiler Monitor";

	Timer timer;

	MonitorGUI() {
		server = new AggressiveModeServer(this);
		server.start();
		currentBrowser = null;
		setTitle(WINDOW_TITLE);
		setBounds(60, 60, 800, 900);
		setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

		menuBar = new JMenuBar();
		JMenu menuFile = new JMenu("File");
		menuFile.setMnemonic('F');
		menuBar.add(menuFile);
		JMenuItem menuConnect = new JMenuItem("Connect");
		menuConnect.setActionCommand("browser.new_connection");
		menuConnect.setMnemonic('C');
		menuConnect.addActionListener(this);
		menuFile.add(menuConnect);
		JMenuItem menuOpenActive = new JMenuItem("Open Active");
		menuOpenActive.setActionCommand("browser.opentree.active");
		menuOpenActive.setMnemonic('O');
		menuOpenActive.addActionListener(this);
		menuFile.add(menuOpenActive);
		JMenuItem menuOpenHeavy = new JMenuItem("Open Heavy");
		menuOpenHeavy.setMnemonic('H');
		menuOpenHeavy.setActionCommand("browser.opentree.heavy");
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
		browserList.addMouseListener(this);
		browserList.addListSelectionListener(this);

		rootPane.setDividerSize(5);
		rootPane.setLeftComponent(browserList);
		rootPane.setRightComponent(new JPanel());
		rootPane.setDividerLocation(240);

		getContentPane().add(rootPane);

		timer = new Timer();
		timer.schedule(new ProfileTimer(), 0, 500);
		timer.schedule(new RedrawTimer(), 0, 500);
	}

	void popupBrowserListMenu(MouseEvent e) {
		JPopupMenu popup = new JPopupMenu();

		JMenuItem item;
		if (currentBrowser != null) {
			if (currentBrowser.isAggressive()) {
				if (!currentBrowser.isActive()) {
					item = new JMenuItem("Connect");
					item.addActionListener(this);
					item.setActionCommand("browser.connect");
					popup.add(item);
				}
			} else {
				item = new JMenuItem("Reconnect");
				item.addActionListener(this);
				item.setActionCommand("browser.reconnect");
				popup.add(item);
			}

			if (currentBrowser.isActive()) {
				item = new JMenuItem("Disconnect");
				item.addActionListener(this);
				item.setActionCommand("browser.disconnect");
				popup.add(item);
			}
		}
		popup.addSeparator();
		item = new JMenuItem("New Connection");
		item.addActionListener(this);
		item.setActionCommand("browser.new_connection");
		popup.add(item);

		item = new JMenuItem("Cancel");
		item.addActionListener(this);
		item.setActionCommand("cancel");
		popup.add(item);

		popup.show(e.getComponent(), e.getX(), e.getY());
	}

	public void mousePressed(MouseEvent e) {
		if (e.getComponent() == browserList) {
			if (e.isPopupTrigger()) {
				popupBrowserListMenu(e);
			}
			if (e.getButton() == MouseEvent.BUTTON1 && e.getClickCount() == 2) {
				actionPerformed(new ActionEvent(e, 0, "browser.defaultact"));
			}
		}

	}

	public void mouseReleased(MouseEvent e) {
	}

	public void mouseClicked(MouseEvent e) {
	}

	public void mouseEntered(MouseEvent e) {
	}

	public void mouseExited(MouseEvent e) {
	}

	public MonitorBrowser newBrowser() {
		MonitorBrowser b = new MonitorBrowser(this);
		browserListModel.addElement(b);
		browserList.repaint();

		return b;
	}

	public void setActiveBrowser(MonitorBrowser b) {
		if (currentBrowser != b) {
			int divloc = rootPane.getDividerLocation();
			setTitle(WINDOW_TITLE + " - " + b.getListLabel());
			rootPane.setRightComponent(b);
			rootPane.setDividerLocation(divloc);
			rootPane.repaint();
			currentBrowser = b;
			browserList.repaint();
		}
	}

	public void actionPerformed(ActionEvent e) {

		if (e.getActionCommand() == "browser.new_connection") {
			ConnectDialog dlg = new ConnectDialog(this);
			dlg.setVisible(true);
		} else if (e.getActionCommand().startsWith("browser.opentree.")) {
			int otm = CallTreeBrowserView.OTM_ACTIVE;
			if(e.getActionCommand() == "browser.opentree.active")
				otm = CallTreeBrowserView.OTM_ACTIVE;
			else if(e.getActionCommand() == "browser.opentree.heavy")
				otm = CallTreeBrowserView.OTM_BOTTLENECK;
				
			currentBrowser.callTreeBrowser.openTree(otm);
			
		} else if (e.getActionCommand() == "Export XML") {
			JFileChooser filechooser = new JFileChooser();

			int selected = filechooser.showSaveDialog(this);
			if (selected == JFileChooser.APPROVE_OPTION) {
				File file = filechooser.getSelectedFile();
				currentBrowser.getMonitor().getDataStore().exportXML(
						file.getAbsolutePath());
				currentBrowser.writeLog("Exported: " + file.getAbsolutePath());
			} else if (selected == JFileChooser.CANCEL_OPTION) {
				currentBrowser.writeLog("Export cancelled");
			} else if (selected == JFileChooser.ERROR_OPTION) {
				currentBrowser.writeLog("Error: Open dialog");
			}
		} else if (e.getActionCommand() == "browser.disconnect") {
			if (currentBrowser != null)
				currentBrowser.close();
		} else if (e.getActionCommand() == "browser.reconnect") {
			if (currentBrowser != null) {
				currentBrowser.close();
				currentBrowser.prepareAgain();
				currentBrowser.startProfile();
			}
		} else if (e.getActionCommand() == "browser.connect") {
			if (currentBrowser != null) {
				currentBrowser.startProfile();
			}
		} else if (e.getActionCommand() == "browser.defaultact") {
			if (currentBrowser != null) {
				if (currentBrowser.isAggressive() && !currentBrowser.isActive()) {
					currentBrowser.startProfile();
				}
				if (!currentBrowser.isAggressive()
						&& !currentBrowser.isActive()) {
					currentBrowser.prepareAgain();
					currentBrowser.startProfile();
				}

			}
		} else {
			System.out.println("Unknown action:" + e.getActionCommand());
		}

	}

	public void connect(String host, int port, int interval) {
		MonitorBrowser b = newBrowser();
		b.preparePassive(host, port, interval);
		setActiveBrowser(b);
		b.startProfile();
	}

	public void newAgressiveConnected(Socket sock) {
		MonitorBrowser b = newBrowser();
		b.prepareAggressive(sock);
		updateBrowserList();
	}

	class ProfileTimer extends TimerTask {
		public void run() {
			if (currentBrowser != null)
				currentBrowser.updateProfiler();
		}
	}

	class RedrawTimer extends TimerTask {
		public void run() {
			if (currentBrowser != null)
				currentBrowser.redraw();
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

	@Override
	public void valueChanged(ListSelectionEvent e) {
		MonitorBrowser b = (MonitorBrowser) browserList.getSelectedValue();
		setActiveBrowser(b);
		System.out.println("Select:" + b.getListLabel());
	}

	public void updateBrowserList() {
		browserList.repaint();
	}
}
