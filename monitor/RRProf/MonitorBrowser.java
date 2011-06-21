package RRProf;

import java.awt.BorderLayout;
import java.awt.Component;
import java.awt.GridLayout;
import java.net.Socket;

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
	MonitorGUI parent;

	boolean aggressive;
	boolean prepared;
	
	MonitorBrowser(MonitorGUI parent) {
		this.parent = parent;
		interval = 1000;
		aggressive = false;
		prepared = false;
	}

	public CallTreeBrowserView getCallTreeBrowser() {
		return callTreeBrowser;
	}
	
	
	public JScrollPane scrollCallTreeBrowser;
	
	public void reset() {
		prepared = false;
		removeAll();
		
		String old_log_text = "[Log]\n";
		if(logText != null)
			old_log_text = logText.getText();

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
		scrollCallTreeBrowser = new JScrollPane();
		scrollCallTreeBrowser.getViewport().setView(callTreeBrowser);
		browserPane.add("Call Tree", scrollCallTreeBrowser);

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
		logText.setText(old_log_text);

		viewerPane = new JTabbedPane();
		viewerPane.add("Log", new JScrollPane(logText));

		JSplitPane sp = new JSplitPane(JSplitPane.VERTICAL_SPLIT);
		sp.setDividerSize(5);
		sp.add(viewerPane, JSplitPane.BOTTOM);
		sp.add(browserPane, JSplitPane.TOP);
		sp.setResizeWeight(1);
		
		// sp.setLeftComponent(browserPane);
		// sp.setRightComponent(viewerPane);

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
		System.out.println("Browser Reset");
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
	
	void newMonitor() {
		mon = new Monitor();
		callTreeBrowser.setMonitor(mon);
		methodBrowser.setMonitor(mon);
	}
	public void startProfile() {
		if(!prepared)
		{
			writeLog("Not prepared");
			return;
		}
		prepared = false;
		if(aggressive)
		{
			mon.startProfileAggressive();
			return;
		}
		else{
			writeLog("Trying to connect " + targetHost + ":"
					+ Integer.toString(targetPort) + "...");
	
			newMonitor();
			mon.startProfile(targetHost, targetPort);
			if (mon.isActive()) {
				writeLog("Connected");
	
			} else {
				writeLog("Error: Failed to connect");
				mon = null;
			}
		}
		repaint();
	}
	public void close() {
		prepared = false;
		if(mon != null)
			mon.close();
	}

	public void preparePassive(String host, int port, int iv) {
		targetHost = host;
		targetPort = port;
		interval = iv;
		reset();
		parent.updateBrowserList();
		prepared = true;
	}
	public void prepareAgain() {
		if(aggressive)
			return;
		reset();
		if(targetHost != null)
			prepared = true;
	}

	public void prepareAggressive(Socket sock) {
		reset();
		aggressive = true;
		newMonitor();
		mon.setAggressive(sock);
		parent.updateBrowserList();
		prepared = true;
	}


	
	public void updateProfiler() {
		methodBrowser.setUpdateMode(true);
		try{
			Monitor target_mon = mon; // 途中で切断された場合でも正常にできるように値をコピーしておく
			if (target_mon != null && target_mon.isActive()) {
				if(target_mon.getProfileData() == null) {
					writeLog("Error: Disconnected");
				}
	
				numRecordsField.setLong(target_mon.getDataStore().numAllNodes());
				numMethodsField.setLong(target_mon.getDataStore().numCallNames());
				inputSizeField.setLong(target_mon.getInputSize());
				outputSizeField.setLong(target_mon.getOutputSize());
			}
			parent.updateBrowserList();
		}
		finally{
			methodBrowser.setUpdateMode(false);
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
	
	public boolean isActive() {
		return mon != null && mon.isActive();
	}

	public String getListLabel() {
		if(aggressive) {
			String label;
			label = mon.getProfileTargetName();
			if(mon.isActive()){
				label += " [Active]";
			}
			return label;
		}
		else if (targetHost != null && targetPort != 0) {
			String label;
			if(mon != null)
				label = mon.getProfileTargetName() + "(" + targetHost + ")";
			else
				label = targetHost;
			if (isActive()) {
				label = label + " [Active]";
			} else {
				label = label + "";
			}
			return label;
		}
		return "<browser>";
	}


	public boolean isAggressive() {
		return aggressive;
	}

}
