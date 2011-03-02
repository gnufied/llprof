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
    ConnectDialog(MonitorGUI owner) {
		super(owner);
		owner_gui = owner;
		
		setTitle("Connect");
		setSize(200, 100);
		
		
		hostField = new JTextField("godel", 15);
		portField = new JTextField("12321", 15);
		intervalField = new JTextField("100", 15);
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
        	owner_gui.connect(
        			hostField.getText(),
        			Integer.parseInt(portField.getText()),
        			Integer.parseInt(intervalField.getText())
        	);
        }
    }
    
}
