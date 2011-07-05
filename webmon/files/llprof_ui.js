
g_current_target = null;
g_max_time_num = 0;
g_active_panel = null;
g_default_panel = "list";
g_realtime_mode = true;
g_select_timenum = 0;
g_target_cct = null;
g_now_info = {};
g_metadata = {};

function assert(cond, msg)
{
    if(!cond)
    {
        alert("Assert failure: " + msg);
    }
}

function html_esc(s){
	s = s.replace("&",'&amp;');
	s = s.replace(">",'&gt;');
	s = s.replace("<",'&lt;');
	return s;
}

function set_realtime_mode(flag)
{
    if(g_realtime_mode == flag)
        return;
    g_realtime_mode = flag;

    g_target_cct = null;

}

function set_select_timenum(v)
{
    g_select_timenum = v;
    if(!g_realtime_mode)
    {
        g_target_cct = {};
    }
}


function update_ui()
{
    $.ajax({
        type: "POST",
        url: "/ds/list",
        dataType: "json",
        data: {},
        success: update_ui_target_list
    });
}

function update_ui_target_list(data)
{
    for(var i = 0; i < data.length; i++)
    {
        if($("#target_item_" + data[i].id).length == 0)
        {
            $("#target_list tbody").append("<tr id='target_item_"+data[i].id+"'><td class='name'></td><td class='host'></td></tr>");
            
            var line = $("#target_item_" + data[i].id);
            line.attr("host_id", data[i].id);
            line.click(function(e){ show_target(line.attr("host_id")); });
        }
        var target_line = $("#target_item_" + data[i].id);
        
        target_line.children(".name").html(data[i].name);
        target_line.children(".host").html(data[i].host);
    }

    if(g_current_target)
    {
        if(g_realtime_mode)
        {
            if(g_target_cct)
            {
                $.ajax({
                    type: "POST",
                    url: "/ds/tree/" + g_current_target + "/diff/" + (g_max_time_num+1),
                    data: {},
                    dataType: "json",
                    success: update_ui_cct,
                });
            }
            else
            {
                g_target_cct = {};
                $.ajax({
                    type: "POST",
                    url: "/ds/tree/" + g_current_target + "/current",
                    data: {},
                    dataType: "json",
                    success: update_ui_cct,
                });
            }
        }else
        {
            $.ajax({
                type: "POST",
                url: "/ds/tree/" + g_current_target + "/diff/" + g_select_timenum + "/" + g_select_timenum,
                data: {},
                dataType: "json",
                success: update_ui_cct,
            });
        }
    }
    else
    {
        setTimeout(update_ui, 1000);
    }
}

function accumurate(dest, src)
{
    for(var i = 0; i < src.all.length; i++)
    {
        if(g_metadata[i].flags.indexOf("S") != -1)
        {
            dest.all[i] = src.all[i];
            dest.cld[i] = src.cld[i];
        }
        else
        {
            dest.all[i] += src.all[i];
            dest.cld[i] += src.cld[i];
        }
    }
}

function set_running_node(thread, flag)
{
    if(!("running_node" in thread))
        return;
    
    var id = thread.running_node;
    
    while(id in thread.nodes)
    {
        var node = thread.nodes[id];
        node.running = flag;
        id = node.pid;
    }
    return;
}

function update_ui_cct(data)
{
    var updateStart = new Date();
    var update_threads = {};
    
    if(g_realtime_mode)
    {
        g_max_time_num = data.timenum;
        $('#timebar').slider("option", "max", g_max_time_num);
    }
    else
    {
        g_target_cct = {};
        if(g_active_panel)
        {
            Panels[g_active_panel].init();
        }
    }
    g_metadata = [];
    for(var i = 0; i < data.numrecords; i++)
    {
        g_metadata[i] = data.metadata[i];
    }

    for(var i = 0; i < data.threads.length; i++)
    {
        var thread = data.threads[i];
        var update_nodes_stocked = [];
        var update_nodes = [];
        
        if(!((thread.id) in g_target_cct))
        {
            g_target_cct[thread.id] = {id: thread.id, nodes: {}};
        }
        
        set_running_node(g_target_cct[thread.id], false);


        $('#debug').html(thread.now_values.join(','));
        g_target_cct[thread.id].now_values = thread.now_values;
        g_target_cct[thread.id].start_values = thread.start_values;
        g_target_cct[thread.id].running_node = thread.running_node;

        for(var j = 0; j < thread.nodes.length; j++)
        {
            var node = thread.nodes[j];
            var node_stock = g_target_cct[thread.id].nodes[node.id];
            if(!node_stock)
            {
                node_stock = {
                    id: node.id,
                    pid: node.pid,
                    cid: {},
                    name: node.name,
                    all: node.all,
                    cld: node.cld,
                    running: false
                };
                g_target_cct[thread.id].nodes[node.id] = node_stock;
                if(node.pid != 1)
                    g_target_cct["" + thread.id].nodes[node.pid].cid[node.id] = node_stock;
            }
            else
            {
                accumurate(node_stock, node);
            }
            update_nodes_stocked.push(node_stock);
            update_nodes.push(node);
        }

        set_running_node(g_target_cct[thread.id], true);

        update_threads[thread.id] = {
            nodes: update_nodes,
            nodes_stocked: update_nodes_stocked,
        };
    }
    var updateStartUpdatePanel = new Date();
    update_main_panel(update_threads);
    var updateEnd = new Date();
    document.title = "Merge: " + (updateStartUpdatePanel - updateStart) + "ms  Pane:" + (updateEnd - updateStartUpdatePanel);
    setTimeout(update_ui, 1000);
}

function show_target(id)
{
    g_target_cct = null;
    g_current_target = id;
    g_max_time_num = 0;
    show_main_panel();
    fit_panel();
}


function show_main_panel(panelname)
{
    if(g_active_panel && Panels[g_active_panel])
    {
        Panels[g_active_panel].hide();
    }
    if(!panelname && !g_active_panel)
        g_active_panel = g_default_panel;
    else if(panelname)
        g_active_panel = panelname;
    Panels[g_active_panel].init();
}

function update_main_panel(updated)
{
    if(g_active_panel)
    {
        Panels[g_active_panel].update(updated);
    }
}


Panels = {};

Panels.cct = {
    
    init: function()
    {
        if(!this.open_nodes)
            this.open_nodes = {};
        $("#mainpanel").html("<div id='cct_outer'><div id='cct_inner'></div></div>");
        for(var thread_id in g_target_cct)
        {
            var thread_elem = Panels.cct.get_thread_elem(thread_id);
            for(var key in g_target_cct[thread_id].nodes)
            {
                var node = g_target_cct[thread_id].nodes[key];
                var node_elem = Panels.cct.get_node_elem(thread_id, node.id, true, node.pid);
                Panels.cct.update_label(node_elem);
            }
        }
        this.last_opened = null;
        this.update_size();
    },

    get_thread_elem: function(thread_id)
    {
        var thread_elem = $("#th_" + thread_id);
        if(thread_elem.length == 0)
        {
            $("#cct_inner").append("<div class='treenode' id='th_" + thread_id + "'><div class='nodelabel'></div><div class='children'></div></div>");
            thread_elem = $("#th_" + thread_id);
            thread_elem.attr("threadid", thread_id);
        }
        thread_elem.children(".nodelabel").html("Thread: " + thread_id);
        return thread_elem;
    },
    
    get_node_elem: function(thread_id, node_id, auto_add, parent_node_id)
    {
        var node_elem_id = "node_th_" + thread_id + "_" + node_id;
        var node_elem = $("#" + node_elem_id);
        if(node_elem.length != 0)
            return node_elem;
        if(auto_add)
        {
            var parent_elem;
            if(parent_node_id == 1)
                parent_elem = Panels.cct.get_thread_elem(thread_id);
            else
                parent_elem = $("#node_th_" + thread_id + "_" + parent_node_id);
            
            if(parent_elem.children(".children").length == 0)
                return null;
            
            Panels.cct.add_node(parent_elem, thread_id, node_id);
            return node_elem;
        }
        else
            return null;
    },
    
    update_thread_label: function(thread_elem)
    {
        if(!thread_elem || thread_elem.length == 0)
            return;
        var thread = g_target_cct[thread_elem.attr('threadid')];
        thread_elem.children(".nodelabel").html("Thread: " + thread.id + " Running:" + thread.running_node + " NowValue:" + thread.now_values.join(', '));
    },
    
    update_label: function(node_elem)
    {
        if(!node_elem || node_elem.length == 0)
            return;
        var thread = g_target_cct[node_elem.attr('threadid')];
        var node = thread.nodes[node_elem.attr('nodeid')];
        
        // ad hoc
        var nsval = node.all[0];
        if(node.running)
        {
            var node_start_time = node.all[1];
            if(thread.start_values && node.all[1] < thread.start_values[1])
            {
                node_start_time = thread.start_values[1];
            }
            
            nsval += (thread.now_values[1] - node_start_time);
        }
        
        if(thread.start_values && nsval > (thread.now_values[1] - thread.start_values[1]))
            nsval = thread.now_values[1] - thread.start_values[1];
        var sval = nsval / (1000 * 1000 * 1000);
        node_elem.children(".nodelabel").html(
            "<img src='/files/"+(node.running?"running":"normal")+".png' />"
             + node.id + ":" + html_esc(node.name) + " " + sval + "s");
        
        // node_elem.children(".nodelabel").html("Node:" + html_esc(node.name) + " v: " + node.all + ";  " + node.cld);
    },

    update_size: function()
    {
        $("#cct_outer")
            .width($("#mainpanel").width()-4)
            .height($("#mainpanel").height()-4);
    },

    update: function(updated)
    {
        for(var thread_id in updated)
        {
            var thread_elem = Panels.cct.get_thread_elem(thread_id);
            Panels.cct.update_thread_label(thread_elem);
            for(var j = 0; j < updated[thread_id].nodes.length; j++)
            {
                var node = updated[thread_id].nodes[j];
                var node_elem = Panels.cct.get_node_elem(thread_id, node.id, true, node.pid);
                Panels.cct.update_label(node_elem);

            }
        }
        Panels.cct.updating = false;
    },
    
    add_node: function(parent_elem, thread_id, node_id){
        var node_elem_id = "node_th_" + thread_id + "_" + node_id;
        parent_elem.children(".children").append("<div class='treenode' id='" + node_elem_id + "'><div class='nodelabel'></div></div>");
        node_elem = $("#" + node_elem_id);
        node_elem.attr("nodeid", node_id);
        node_elem.attr("threadid", thread_id);
        Panels.cct.update_label(node_elem);
        
        node_elem.children(".nodelabel")
            .click(function(e){
                    Panels.cct.open_node_elem($(this).parent()); 
                    e.preventDefault();
            })
            .mouseenter(function(e){
                $(this).addClass("nodelabel_hover");
            })
            .mouseleave(function(e){
                $(this).removeClass("nodelabel_hover");
            })
        ;
        if(node_id in this.open_nodes)
        {
            this.open_node_elem(node_elem);
        }
        return node_elem;
    },
    
    open_node_elem: function(elem)
    {
        if(elem.children(".children").length != 0)
        {
            delete this.open_nodes[elem.attr("nodeid")];
            elem.children(".children").remove();
            return;
        }
        elem.append("<div class='children'></div>");
        this.last_opened = {threadid: elem.attr("threadid"), nodeid: elem.attr("nodeid")};

        var thread_id = this.last_opened.threadid;
        var node = g_target_cct[thread_id].nodes[this.last_opened.nodeid];
        this.open_nodes[this.last_opened.nodeid] = node;
        for(var id in node.cid)
        {
            var node_elem = Panels.cct.add_node(elem, thread_id, id);
        }
    },
    
    hide: function()
    {
        
    }
};


Panels.list = {
    
    init: function()
    {
        $("#mainpanel").html("<table></table><div><table id='mtbl'></table></div>");
        this.list = {
        };
        this.ordered = [];
        this.num_items = 0;
        this.unordered = [];

        for(var thread_id in g_target_cct)
        {
            for(var key in g_target_cct[thread_id].nodes)
            {
                var node = g_target_cct[thread_id].nodes[key];
                this.update_item(thread_id, node);
            }
        }
    },
    update_size: function()
    {
    },
    
    update_item: function(thread_id, node)
    {
        var item_id = "li_" + thread_id + "_" + node.name;
        if(!(item_id in this.list))
        {
            var node_stock = this.list[item_id] = {
                name: node.name,
                id: node.id,
                all: new Array(g_metadata.length),
                cld: new Array(g_metadata.length),
                opos: -2
            };
            for(var i = 0; i < g_metadata.length; i++)
                node_stock.all[i] = node_stock.cld[i] = 0;
            this.unordered.push(node_stock);
            this.num_items++;
            $('#mtbl').append("<tr id='tbl_"+(this.num_items-1)+"'><td class='col_1'></td><td class='col_2'></td><td class='col_3'></td></tr>");
        }
        else
        {
            node_stock = this.list[item_id];
            if(node_stock.opos >= 0)
            {
                assert(this.ordered[node_stock.opos].id == node_stock.id, "Id not match");
                node_stock.opos = -1;
            }
        }
        accumurate(node_stock, node);
    },

    merge_orderd: function()
    {
        var cmp_func = function(a,b) {return -(a.all[0] - b.all[0]);};

        for(var i = 0; i < this.ordered.length; i++)
        {
            if(this.ordered[i].opos < 0)
            {
                this.unordered.push(this.ordered[i]);
                this.ordered.splice(i, 1);
                i--;
            }
        }

        this.unordered.sort(cmp_func);
        for(var i = 0; i < this.ordered.length; i++)
        {
            if(this.unordered.length > 0 && cmp_func(this.ordered[i], this.unordered[0]) > 0)
            {
                this.ordered.splice(i, 0, this.unordered.shift());
            }
            this.ordered[i].opos = i;
        }
        while(this.unordered.length != 0)
        {
            var node_stock = this.unordered.shift();
            node_stock.opos = this.ordered.length;
            this.ordered.push(node_stock);
        }

    },

    update: function(updated)
    {
        for(var thread_id in updated)
        {
            var thread_elem = Panels.cct.get_thread_elem(thread_id);
            Panels.cct.update_thread_label(thread_elem);
            for(var j = 0; j < updated[thread_id].nodes.length; j++)
            {
                var node = updated[thread_id].nodes[j];
                Panels.list.update_item(thread_id, node);
            }
        }
        this.merge_orderd();

        for(var i = 0; i < this.ordered.length && i < 30; i++)
        {
            var node_stock = this.ordered[i];
            $("#tbl_"+i).children("tbody .col_1").html(node_stock.name);
            $("#tbl_"+i).children("tbody .col_2").html(node_stock.all[0]);
            $("#tbl_"+i).children("tbody .col_3").html(node_stock.all[1]);
        }
    },
    
    hide: function()
    {
        
    }
};



Panels.square = {
    
    init: function()
    {
        if(!this.open_nodes)
            this.open_nodes = {};
        $("#mainpanel").html("<div id='cct_outer'><div id='cct_inner'></div></div>");
        for(var thread_id in g_target_cct)
        {
            var thread_elem = Panels.cct.get_thread_elem(thread_id);
            for(var key in g_target_cct[thread_id].nodes)
            {
                var node = g_target_cct[thread_id].nodes[key];
                var node_elem = Panels.cct.get_node_elem(thread_id, node.id, true, node.pid);
                Panels.cct.update_label(node_elem);
            }
        }
        this.last_opened = null;
        this.update_size();
    },

    get_thread_elem: function(thread_id)
    {
        var thread_elem = $("#th_" + thread_id);
        if(thread_elem.length == 0)
        {
            $("#cct_inner").append("<div class='treenode' id='th_" + thread_id + "'><div class='nodelabel'></div><div class='children'></div></div>");
            thread_elem = $("#th_" + thread_id);
            thread_elem.attr("threadid", thread_id);
        }
        thread_elem.children(".nodelabel").html("Thread: " + thread_id);
        return thread_elem;
    },
    
    get_node_elem: function(thread_id, node_id, auto_add, parent_node_id)
    {
        var node_elem_id = "node_th_" + thread_id + "_" + node_id;
        var node_elem = $("#" + node_elem_id);
        if(node_elem.length != 0)
            return node_elem;
        if(auto_add)
        {
            var parent_elem;
            if(parent_node_id == 1)
                parent_elem = Panels.cct.get_thread_elem(thread_id);
            else
                parent_elem = $("#node_th_" + thread_id + "_" + parent_node_id);
            
            if(parent_elem.children(".children").length == 0)
                return null;
            
            Panels.cct.add_node(parent_elem, thread_id, node_id);
            return node_elem;
        }
        else
            return null;
    },
    
    update_thread_label: function(thread_elem)
    {
        if(!thread_elem || thread_elem.length == 0)
            return;
        var thread = g_target_cct[thread_elem.attr('threadid')];
        thread_elem.children(".nodelabel").html("Thread: " + thread.id + " Running:" + thread.running_node + " NowValue:" + thread.now_values.join(', '));
    },
    
    update_label: function(node_elem)
    {
        if(!node_elem || node_elem.length == 0)
            return;
        var thread = g_target_cct[node_elem.attr('threadid')];
        var node = thread.nodes[node_elem.attr('nodeid')];
        
        // ad hoc
        var nsval = node.all[0];
        if(node.running)
        {
            var node_start_time = node.all[1];
            if(thread.start_values && node.all[1] < thread.start_values[1])
            {
                node_start_time = thread.start_values[1];
            }
            
            nsval += (thread.now_values[1] - node_start_time);
        }
        
        if(thread.start_values && nsval > (thread.now_values[1] - thread.start_values[1]))
            nsval = thread.now_values[1] - thread.start_values[1];
        var sval = nsval / (1000 * 1000 * 1000);
        node_elem.children(".nodelabel").html("<img src='/files/"+(node.running?"running":"normal")+".png' />" + html_esc(node.name) + " " + sval + "s");
        
        // node_elem.children(".nodelabel").html("Node:" + html_esc(node.name) + " v: " + node.all + ";  " + node.cld);
    },

    update_size: function()
    {
        $("#cct_outer")
            .width($("#mainpanel").width()-4)
            .height($("#mainpanel").height()-4);
    },

    update: function(updated)
    {
        for(var thread_id in updated)
        {
            var thread_elem = Panels.cct.get_thread_elem(thread_id);
            Panels.cct.update_thread_label(thread_elem);
            for(var j = 0; j < updated[thread_id].nodes.length; j++)
            {
                var node = updated[thread_id].nodes[j];
                var node_elem = Panels.cct.get_node_elem(thread_id, node.id, true, node.pid);
                Panels.cct.update_label(node_elem);

            }
        }
        Panels.cct.updating = false;
    },
    
    add_node: function(parent_elem, thread_id, node_id){
        var node_elem_id = "node_th_" + thread_id + "_" + node_id;
        parent_elem.children(".children").append("<div class='treenode' id='" + node_elem_id + "'><div class='nodelabel'></div></div>");
        node_elem = $("#" + node_elem_id);
        node_elem.attr("nodeid", node_id);
        node_elem.attr("threadid", thread_id);
        Panels.cct.update_label(node_elem);
        
        node_elem.children(".nodelabel")
            .click(function(e){
                    Panels.cct.open_node_elem($(this).parent()); 
                    e.preventDefault();
            })
            .mouseenter(function(e){
                $(this).addClass("nodelabel_hover");
            })
            .mouseleave(function(e){
                $(this).removeClass("nodelabel_hover");
            })
        ;
        if(node_id in this.open_nodes)
        {
            this.open_node_elem(node_elem);
        }
        return node_elem;
    },
    
    open_node_elem: function(elem)
    {
        if(elem.children(".children").length != 0)
        {
            delete this.open_nodes[elem.attr("nodeid")];
            elem.children(".children").remove();
            return;
        }
        elem.append("<div class='children'></div>");
        this.last_opened = {threadid: elem.attr("threadid"), nodeid: elem.attr("nodeid")};

        var thread_id = this.last_opened.threadid;
        var node = g_target_cct[thread_id].nodes[this.last_opened.nodeid];
        this.open_nodes[this.last_opened.nodeid] = node;
        for(var id in node.cid)
        {
            var node_elem = Panels.cct.add_node(elem, thread_id, id);
        }
    },
    
    hide: function()
    {
        
    }
};



function fit_panel()
{
    var win_h = $(window).height();
    var win_w = $(window).width();

    $('#mainpanel').width(win_w - 200 - 10);
    $('#mainpanel').height(win_h - 120 - 10);

    $('#modepanel').width(win_w - 200 - 10);
    $('#modepanel').height(120);


    $('#sidepanel').width(200);
    $('#sidepanel').height(win_h - 10);
    
    if(g_active_panel)
        Panels[g_active_panel].update_size();
}

function init_modepanel()
{
    $('#timebar').slider({
        min: 0,
        max: 10,
        startValue: 0,
        slide: function(e, ui){
            set_select_timenum(ui.value);
        }
    });
    $('#timewidthbar').slider({
        min: 50,
        max: 300,
        startValue: 100,
    });

}

function do_command(cmd)
{
    var cmd = cmd.split(" ");
    
    if(cmd[0] == "g")
    {
        g_active_panel = null;
        $('#mainpanel').html("This is graph");
        
        fetch_table_data({
            nodeid: cmd[1],
            start_time: 0,
            end_time: 100,
            cb: function(data){
                alert("Got data");
                var html = "";
                for(var i = 0; i < data.length; i++)
                {
                    html += "*** Line " + i + "<br />";
                    for(var key in data[i])
                    {
                        html += "" + key + " = " + data[i][key] + "<br />";
                    }
                }
                $('#mainpanel').html(html);
            },
        });
    }
}

function fetch_table_data(req)
{
    if(!req.initialized)
    {
        req.initialized = true;
        req.data = [];
        req.keys = {};
        req.keys_idx = [];
        req.numkeys = 0;
    }
    if(req.start_time >= req.end_time)
    {
        req.cb(req.data);
        return;
    }
    $.ajax({
        type: "POST",
        url: "/ds/tree/" + g_current_target + "/diff/" + req.start_time + "/" + req.start_time,
        data: {},
        dataType: "json",
        success: function(data){
            if(!(data.threads[0]))
            {
                alert("No Node");
                return;
            }
            var thread = data.threads[0];
            
            var current = {};
            for(var nodeid in thread.nodes)
            {
                var node = thread.nodes[nodeid];
                if(""+node.pid == req.nodeid)
                {
                    current[node.id] = node.all;
                    if(!(node.id in req.keys))
                    {
                        req.keys[node.id] = {
                            idx: req.numkeys,
                            id: node.id,
                            name: node.name,
                        };
                        req.keys_idx.push(req.keys);
                        req.numkeys++;
                    }
                }
            }
            req.data.push(current);
            req.start_time += 1;
            fetch_table_data(req);
        },
        error: function(){
            alert("Data Error");
        }
    });
}



$(function(){
    setTimeout(update_ui, 500);
    fit_panel();
    $(window).resize(fit_panel);
    init_modepanel();
    
    $("#view_select").change(function(){
        show_main_panel($(this).val());
    });
    
    $('#check_realtime').click(function(){
        if($(this).attr("checked"))
            set_realtime_mode(true);
        else
            set_realtime_mode(false);
    });
    
    $('#command_do').click(function(){
        do_command($('#command_text').val());
    });
    
    $("#connect_button").click(function(){
        $.ajax({
            type: "POST",
            url: "/ds/new",
            data: {"host": $('#server_address').val()},
            dataType: "json",
            success: function(data){
            }
        });
    });
});
