package RRProf;

import java.util.*;
import java.util.Timer;
import java.io.*;
import java.awt.*;
import java.awt.event.*;

import javax.swing.*;
import javax.swing.event.*;
import javax.swing.tree.*;



public class MonitorGUI extends JFrame implements ActionListener {
	private Monitor mon;
	
	JTabbedPane browserPane, viewerPane;
    JTable childrenTable;
    JPanel statisticsPane;
    
    CallTreeBrowserView callTreeBrowser;
    MethodBrowser methodBrowser;
    
    public class StatisticField{
    	Object value;
    	public JLabel label;
    	
    	public void updateLabel() {
    		if(value == null)
    			label.setText("(null)");
    		else
    			label.setText(value.toString());
    	}
    	public void setLong(long val) {
    		value = val;
    		updateLabel();
    	}
    	public void addLong(long val) {
    		value = (Long)value + val;
    		updateLabel();
    	}
    }
    
    
    StatisticField numRecordsField, numMethodsField, inputSizeField, outputSizeField;

    MonitorGUI(){
		setTitle("rrprof");
		setBounds(60, 60, 800, 900);
		setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		
        callTreeBrowser = new CallTreeBrowserView();
        //callTreeBrowser.setMeasureMode(null, 1);
        callTreeBrowser.setMeasureMode("ms", 1000*1000);
        JScrollPane ct_scrollPane = new JScrollPane();
        ct_scrollPane.getViewport().setView(callTreeBrowser);

        methodBrowser = new MethodBrowser(this);
        JScrollPane mb_scrollPane = new JScrollPane();
        mb_scrollPane.getViewport().setView(methodBrowser);

        browserPane = new JTabbedPane();
        browserPane.add("Call Tree", ct_scrollPane);
        browserPane.add("Method", mb_scrollPane);
        
        statisticsPane = new JPanel();
        statisticsPane.setLayout(new GridLayout(10,0));

        browserPane.add("Profiler info", statisticsPane);
        
        viewerPane = new JTabbedPane();

		JSplitPane sp = new JSplitPane(JSplitPane.VERTICAL_SPLIT);
        sp.setDividerSize(5);
        sp.setLeftComponent(browserPane);
        sp.setRightComponent(viewerPane);

		getContentPane().add(sp);
        
        JMenuBar menuBar = new JMenuBar();
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
        
        numRecordsField = addStatisticsField("numRecords", "Number of records", new Long(0));
        numMethodsField = addStatisticsField("numMethods", "Number of methods", new Long(0));
        inputSizeField = addStatisticsField("inputSize", "Input size", new Long(0));
        outputSizeField = addStatisticsField("outputSize", "Output size", new Long(0));
	}
    public StatisticField addStatisticsField(String field_id, String label, Object value) {
    	StatisticField field = new StatisticField();
    	field.label = new JLabel();
    	field.value = value;
    	field.updateLabel();
    	
    	JPanel panel = new JPanel();
    	panel.setLayout(new GridLayout(1,2));
    	panel.add(new JLabel(label));
    	panel.add(field.label);
    	statisticsPane.add(panel);
    	return field;
    	
    }
    
	public void actionPerformed(ActionEvent e) {
		if(e.getActionCommand() == "Connect")
        {
			ConnectDialog dlg = new ConnectDialog(this);
			dlg.setVisible(true);
        }
		else if(e.getActionCommand() == "Open Active")
        {
			callTreeBrowser.openTree(CallTreeBrowserView.OTM_ACTIVE);
        }
		else if(e.getActionCommand() == "Open Heavy")
        {
			callTreeBrowser.openTree(CallTreeBrowserView.OTM_BOTTLENECK);
        }
		else if(e.getActionCommand() == "Export XML")
        {
		    JFileChooser filechooser = new JFileChooser();

		    int selected = filechooser.showSaveDialog(this);
		    if (selected == JFileChooser.APPROVE_OPTION){
		      File file = filechooser.getSelectedFile();
		      mon.getDataStore().exportXML(file.getName());
		    }else if (selected == JFileChooser.CANCEL_OPTION){
		    }else if (selected == JFileChooser.ERROR_OPTION){
		    	System.out.println("Error: Open dialog");
		    }
			
        }

	}
	
	public void connect(String host, int port, int interval) {
		mon = new Monitor();
		callTreeBrowser.setMonitor(mon);
		
		methodBrowser.setMonitor(mon);
		mon.startProfile(host, port);
		if(mon.is_connected()){
			Timer t = new Timer();
			t.schedule(new ProfileTimer(), 0, interval);
		}
	}

	public void openRecordSetView(DataStore.RecordSet recset)
	{
		RecordSetViewer view = new RecordSetViewer(recset);
		viewerPane.add("Method", view);
	}
	
    class ProfileTimer extends TimerTask {
        public void run(){
        	if(mon.is_connected()){
	        	try{
	        		mon.getProfileData();
	        		// mon.printProfileData();
	        		
	        		numRecordsField.setLong(mon.getDataStore().numAllNodes());
	        		numMethodsField.setLong(mon.getDataStore().numCallNames());
	        		inputSizeField.setLong(mon.getInputSize());
	        		outputSizeField.setLong(mon.getOutputSize());
	        	}
	    		catch(IOException e)
	    		{
	    			e.printStackTrace();
	    		}
        	}
        }
    }
	
	public static void main(String[] args) throws Exception {
		MonitorGUI frame = new MonitorGUI();
	    frame.setVisible(true);
	    
	    if(args.length > 2)
	    {
	    	frame.connect(args[0], Integer.parseInt(args[1]), Integer.parseInt(args[2]));
	    }
	    
	}
	
}
