package RRProf;

import java.awt.BorderLayout;
import java.awt.Component;
import java.awt.GridLayout;

import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JSplitPane;
import javax.swing.JTabbedPane;
import javax.swing.JTable;
import javax.swing.JTextArea;


public class MonitorBrowser extends JPanel {
	public class StatisticField {
		Object value;
		public JLabel label;

		public void updateLabel() {
			if (value == null)
				label.setText("(null)");
			else
				label.setText(value.toString());
		}

		public void setLong(long val) {
			value = val;
			updateLabel();
		}

		public void addLong(long val) {
			value = (Long) value + val;
			updateLabel();
		}
	}

	private Monitor mon;
	String targetHost;
	int targetPort;
	int interval;

	public Monitor getMonitor(){return mon;}
	
	JTabbedPane browserPane, viewerPane;
	JTable childrenTable;
	JPanel statisticsPane;
	CallTreeBrowserView callTreeBrowser;
	MethodBrowser methodBrowser;
	StatisticField numRecordsField, numMethodsField, inputSizeField,
			outputSizeField;

	SquaresView squaresView;
	
	JTextArea logText;

	MonitorBrowser() {
		interval = 1000;
	}

	
	public void reset() {
		removeAll();

		if (methodBrowser != null)
			methodBrowser.close();
		methodBrowser = null;

		if (mon != null)
			mon.close();
		mon = null;
		setLayout(new BorderLayout());
		
		browserPane = new JTabbedPane();

		// コールツリーページ
		callTreeBrowser = new CallTreeBrowserView();
		callTreeBrowser.setMeasureMode("ms", 1000 * 1000);
		JScrollPane ct_scrollPane = new JScrollPane();
		ct_scrollPane.getViewport().setView(callTreeBrowser);
		browserPane.add("Call Tree", ct_scrollPane);

		// メソッドリストページ
		methodBrowser = new MethodBrowser(this);
		JScrollPane mb_scrollPane = new JScrollPane();
		mb_scrollPane.getViewport().setView(methodBrowser);
		browserPane.add("Method", mb_scrollPane);

		// Squaresページ
		squaresView = new SquaresView(this);
		browserPane.add("Squares", squaresView);
		
		// 統計情報ページ
		statisticsPane = new JPanel();
		statisticsPane.setLayout(new GridLayout(10, 0));
		browserPane.add("Profiler info", statisticsPane);

		logText = new JTextArea();
		logText.setEditable(false);
		viewerPane = new JTabbedPane();
		viewerPane.add("Log", new JScrollPane(logText));

		JSplitPane sp = new JSplitPane(JSplitPane.VERTICAL_SPLIT);
		sp.setDividerSize(5);
		sp.setLeftComponent(browserPane);
		sp.setRightComponent(viewerPane);

		add(sp);
		numRecordsField = addStatisticsField("numRecords", "Number of records",
				new Long(0));
		numMethodsField = addStatisticsField("numMethods", "Number of methods",
				new Long(0));
		inputSizeField = addStatisticsField("inputSize", "Input size",
				new Long(0));
		outputSizeField = addStatisticsField("outputSize", "Output size",
				new Long(0));

		invalidate();
		validate();
		repaint();

	}

	public void writeLog(String s) {
		logText.append(s + "\n");
	}

	public void openRecordSetView(DataStore.RecordSet recset) {
		RecordSetViewer view = new RecordSetViewer(recset);
		viewerPane.add("Method", view);
	}

	public StatisticField addStatisticsField(String field_id, String label,
			Object value) {
		StatisticField field = new StatisticField();
		field.label = new JLabel();
		field.value = value;
		field.updateLabel();

		JPanel panel = new JPanel();
		panel.setLayout(new GridLayout(1, 2));
		panel.add(new JLabel(label));
		panel.add(field.label);
		statisticsPane.add(panel);
		return field;

	}

	public void connect() {
		reset();
		writeLog("Trying to connect " + targetHost + ":"
				+ Integer.toString(targetPort) + "...");
		mon = new Monitor();
		System.out.println("New mon MID:" + mon.getMID());
		callTreeBrowser.setMonitor(mon);

		methodBrowser.setMonitor(mon);
		mon.startProfile(targetHost, targetPort);
		if (mon.isConnected()) {
			writeLog("Connected");

		} else {
			writeLog("Error: Failed to connect");
			mon = null;
		}
		repaint();
	}

	public void connect(String host, int port, int iv) {
		targetHost = host;
		targetPort = port;
		this.interval = iv;
		connect();
	}

	public void updateProfiler() {
		Monitor target_mon = mon; // 途中で切断された場合でも正常にできるように値をコピーしておく
		if (target_mon != null && target_mon.isConnected()) {
			if(target_mon.getProfileData() == null) {
				writeLog("Error: Disconnected (MID:" + target_mon.getMID() + ")");
			}

			numRecordsField.setLong(target_mon.getDataStore().numAllNodes());
			numMethodsField.setLong(target_mon.getDataStore().numCallNames());
			inputSizeField.setLong(target_mon.getInputSize());
			outputSizeField.setLong(target_mon.getOutputSize());
		}
	}

	public void redraw() {
		try{
			SquaresView view = (SquaresView)browserPane.getSelectedComponent();
			if(view != null)
				view.repaint();
		}catch(ClassCastException e)
		{
			// do nothing
		}
	}
	
	public boolean isConnected() {
		return mon != null && mon.isConnected();
	}

	public String getListLabel() {
		if (targetHost != null && targetPort != 0) {
			String label = targetHost + ":" + Integer.toString(targetPort);
			if (isConnected()) {
				label = label + " (Connected)";
			} else {
				label = label + "";
			}
			return label;
		}
		return "<browser>";
	}

}
