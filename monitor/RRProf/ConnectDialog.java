package RRProf;

import java.util.*;
import java.io.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.event.*;
import javax.swing.tree.*;


public class ConnectDialog extends JDialog implements ActionListener {
    JTextField hostField, intervalField, portField;
    MonitorGUI owner_gui;
    
    Properties loadProperties()
    {
		Properties option_file = new Properties();
		try{
			option_file.load(new FileInputStream("rrprof_monitor.properties"));  
		}
		catch(IOException exc)
		{
		}
		return option_file;
    }
    
    ConnectDialog(MonitorGUI owner) {
		super(owner);
		owner_gui = owner;
		
		setTitle("Connect");
		setSize(200, 100);
		
		Properties option_file = loadProperties();
		
		hostField = new JTextField(option_file.getProperty("monitor.host", "localhost"), 15);
		portField = new JTextField(option_file.getProperty("monitor.port", "12321"), 15);
		intervalField = new JTextField(option_file.getProperty("monitor.interval", "500"), 15);
		JButton connectBtn = new JButton("Connect");
		connectBtn.addActionListener(this);
		getContentPane().setLayout(new GridLayout(4,2));
		getContentPane().add(new JLabel("Host"));
		getContentPane().add(hostField);
		getContentPane().add(new JLabel("Port"));
		getContentPane().add(portField);
		getContentPane().add(new JLabel("Interval"));
		getContentPane().add(intervalField);
		getContentPane().add(connectBtn);

    }
    public void actionPerformed(ActionEvent e){
       setVisible(false);
       if(e.getActionCommand() == "Connect")
        {
     	   Properties option_file = loadProperties();
     	   option_file.setProperty("monitor.host", hostField.getText());
     	   option_file.setProperty("monitor.port", portField.getText());
     	   option_file.setProperty("monitor.interval", intervalField.getText());
     	   try{
     		   option_file.store(new FileOutputStream("rrprof_monitor.properties"), "Monitor Option");
	   		}
	   		catch(IOException exc)
	   		{
	   		}
        	owner_gui.connect(
        			hostField.getText(),
        			Integer.parseInt(portField.getText()),
        			Integer.parseInt(intervalField.getText())
        	);
        }
    }
    
}
