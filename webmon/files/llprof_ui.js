
g_current_target = null;
g_now_time_num = 0;
g_active_panel = null;
g_default_panel = "cct";

g_target_cct = null;
g_now_info = {};
g_metadata = {};

function html_esc(s){
	s = s.replace("&",'&amp;');
	s = s.replace(">",'&gt;');
	s = s.replace("<",'&lt;');
	return s;
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
        if(g_target_cct)
        {
            $.ajax({
                type: "POST",
                url: "/ds/tree/" + g_current_target + "/diff/" + g_now_time_num,
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
    }
    else
    {
        setTimeout(update_ui, 1000);
    }
}

function accumurate(dest, src)
{
    for(var i = 0; i < src.length; i++)
    {
        if(g_metadata[i].flags.indexOf("A") != -1)
            dest[i] = src[i];
        else
            dest[i] += src[i];
    }
}

function update_ui_cct(data)
{
    var updateStart = new Date();
    var update_threads = {};
    g_now_time_num = data.timenum;
    g_metadata = data.metadata;

    for(var i = 0; i < data.threads.length; i++)
    {
        var thread = data.threads[i];
        var update_nodes = [];
        if(!((thread.id) in g_target_cct))
        {
            g_target_cct[thread.id] = {id: thread.id, nodes: {}};
        }
        
        g_target_cct[thread.id].now_values = thread.now_values;
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
                    values: node.values
                };
                g_target_cct[thread.id].nodes[node.id] = node_stock;
                if(node.pid != 1)
                    g_target_cct["" + thread.id].nodes[node.pid].cid[node.id] = node_stock;
            }
            else
            {
                accumurate(node_stock.values, node.values);
            }
            update_nodes.push(node_stock);
        }
        update_threads[thread.id] = update_nodes;
    }
    update_main_panel(update_threads);
    setTimeout(update_ui, 1000);
    var timedur = (new Date()) - updateStart;
    document.title = "Time: " + timedur + "ms";
}

function show_target(id)
{
    g_target_cct = null;
    g_current_target = id;
    $(".main").html("<div id='cct_outer'><div id='cct_inner'></div></div>");
    show_main_panel(g_default_panel);
    fit_panel();
}

function show_main_panel(panelname)
{
    g_active_panel = panelname;
    Panels[g_active_panel].show();
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
        var node = g_target_cct[node_elem.attr('threadid')].nodes[node_elem.attr('nodeid')];
        node_elem.children(".nodelabel").html("Node:" + html_esc(node.name) + " times = " + node.values);
    },
    
    show: function()
    {
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
            for(var j = 0; j < updated[thread_id].length; j++)
            {
                var node = updated[thread_id][j];
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
            .click(function(){
                    Panels.cct.open_node_elem($(this).parent()); 
            });
        return node_elem;
    },
    
    open_node_elem: function(elem)
    {
        if(elem.children(".children").length != 0)
        {
            elem.children(".children").remove();
            return;
        }
        elem.append("<div class='children'></div>");
        var thread_id = elem.attr("threadid");
        var node = g_target_cct[thread_id].nodes[elem.attr("nodeid")];
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
    $('#mainpanel').height(win_h - 10);
    $('#sidepanel').width(200);
    $('#sidepanel').height(win_h - 10);
    
    if(g_active_panel)
        Panels[g_active_panel].update_size();
}

$(function(){
    setTimeout(update_ui, 500);
    fit_panel();
    $(window).resize(fit_panel);
    
    $("#connect_button").click(function(){
        $.ajax({
            type: "POST",
            url: "/ds/new",
            data: {"host": $('#server_address').val()},
            dataType: "json",
            success: function(data){
                alert("Connected:" + data);
            }
        });
    });
});
