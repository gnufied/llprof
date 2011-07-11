
g_current_target = null;
g_active_panel = null;
g_default_panel = "cct";
g_target_cct = null;
g_now_info = {};
g_metadata = {};

g_timenav = {
    real_time: true,
    select_time: 0,
    select_width: 0,
    max_time: 0
}

g_need_update_counter = 0;

g_colors = [
    "#FFE",
    "#FEF",
    "#EFF",
    "#EFE",
    "#EEF",
    "#FEE",
    "#FFC",
    "#FCF",
    "#CFF"
];


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




function count_update_counter()
{
    g_need_update_counter--;
    if(g_need_update_counter < 0)
    {
        g_need_update_counter = 5;
        update_ui();
    }
    else
    {
        setTimeout(count_update_counter, 200);
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
    fit_panel();
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
        if(g_timenav.real_time)
        {
            if(g_target_cct)
            {
                $.ajax({
                    type: "POST",
                    url: "/ds/tree/" + g_current_target + "/diff/" + (g_timenav.max_time+1),
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
            var time_selection_start = g_timenav.select_time + 1 - g_timenav.select_width;
            if(time_selection_start > g_timenav.select_time)
                time_selection_start = g_timenav.select_time;
            $.ajax({
                type: "POST",
                url: "/ds/tree/" + g_current_target + "/diff/" + time_selection_start + "/" + g_timenav.select_time,
                data: {},
                dataType: "json",
                success: update_ui_cct,
            });
        }
    }
    else
    {
        count_update_counter();
    }
}

function accumurate(dest, src, thread)
{
    for(var i = 0; i < src.all.length; i++)
    {
        if(g_metadata[i].flags.indexOf("S") != -1)
        {
            dest.all[i] = get_profile_value_all(thread, src, i);
            dest.cld[i] = get_profile_value_cld(thread, src, i);
        }
        else
        {
            dest.all[i] += get_profile_value_all(thread, src, i);
            dest.cld[i] += get_profile_value_cld(thread, src, i);
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

function fmt_date(d)
{
    yy = d.getYear();
    mm = d.getMonth() + 1;
    dd = d.getDate();
    hh = d.getHours();
    min = d.getMinutes();
    sec = d.getSeconds();
    if (yy < 2000) { yy += 1900; }
    if (mm < 10) { mm = "0" + mm; }
    if (dd < 10) { dd = "0" + dd; }
    return "" + yy + "/" + mm + "/" + dd + " " + hh + ":" + min + ":" + sec;
}

function update_ui_cct(data)
{
    var updateStart = new Date();
    var update_threads = {};
    
    if(g_timenav.real_time)
    {
        g_timenav.max_time = data.timenum;
        $('#timebar').slider("option", "max", g_timenav.max_time);
    }
    else
    {
        g_target_cct = {};
    }
    g_metadata = [];
    for(var i = 0; i < data.numrecords; i++)
    {
        g_metadata[i] = data.metadata[i];
    }
    
    var timecaption_updated = false;
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


        g_target_cct[thread.id].now_values = thread.now_values;
        g_target_cct[thread.id].start_values = thread.start_values;
        g_target_cct[thread.id].running_node = thread.running_node;

        if(!timecaption_updated)
        {
            timecaption_updated = true;
            if(thread.now_values[1])
            {
                var msval = parseInt(thread.now_values[1]) / 1000 / 1000;
                $('#timebar_timecaption').html(fmt_date(new Date(msval)));
            }
        }

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

    if(g_timenav.real_time)
    {
        update_main_panel(update_threads);
    }
    else
    {
        if(g_active_panel)
            Panels[g_active_panel].tree_reload();
    }
    var updateEnd = new Date();
    document.title =
        "Merge: " + (updateStartUpdatePanel - updateStart) + "ms "
        + " Pane:" + (updateEnd - updateStartUpdatePanel)
        + " nDiv:" + ($('div').length)
    ;
    count_update_counter();
}

function show_target(id)
{
    g_target_cct = null;
    g_current_target = id;
    g_timenav.max_time = 0;
    update_nav();
    fit_panel();
}



function update_main_panel(updated)
{
    if(g_active_panel)
    {
        Panels[g_active_panel].update(updated);
    }
}



function get_profile_value_all(thread, node, index)
{
    if(index != 0)
        return node.all[index];

    var val = node.all[0];
    if(node.running)
    {
        var node_start_time = node.all[1];
        if(!g_timenav.real_time && thread.start_values && node.all[1] < thread.start_values[1])
        {
            node_start_time = thread.start_values[1];
        }
        
         val += (thread.now_values[1] - node_start_time);
    }
    
    if(!g_timenav.real_time && thread.start_values &&  val > (thread.now_values[1] - thread.start_values[1]))
         val = thread.now_values[1] - thread.start_values[1];
    return val;
}

function get_profile_value_cld(thread, node, index)
{
    return node.cld[index];
}

function get_profile_value_self(thread, node, index)
{
    return get_profile_value_all(thread, node, index) - get_profile_value_cld(thread, node, index);
}


Panels = {};

Panels.cct = {
    
    init: function()
    {
        if(!this.open_nodes)
            this.open_nodes = {};
    },
    tree_reload: function()
    {
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
        thread_elem.children(".nodelabel").html("Thread: " + thread.id); 
            // + " Running:" + thread.running_node + " NowValue:" + thread.now_values.join(', '));
    },
    
    update_label: function(node_elem)
    {
        if(!node_elem || node_elem.length == 0)
            return;
        var thread = g_target_cct[node_elem.attr('threadid')];
        var node = thread.nodes[node_elem.attr('nodeid')];
        
        var sval = get_profile_value_all(thread, node, 0) / (1000 * 1000 * 1000);
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
        this.namecol = {
            width: 200
        };
        this.col = [
            {idx: 0, mode: "all", width: 140},
            {idx: 0, mode: "self", width: 140}
        ];
        $("#mainpanel").html("<table id='mtbl_header'></table><div id='mtbl_scroll'><table id='mtbl'></table></div>");
        $('#mtbl_scroll').css('overflow', 'scroll');
    },

    update_column: function()
    {
        $('#mtbl_header').html("<th id='lhead_name'>name</th>");
        $("#list_body_head").html("<th id='lbodyhead_name'></th>");
        
        $('#lhead_name').width(this.namecol.width);
        $('#lbodyhead_name').width(this.namecol.width);
        for(var i = 0; i < this.col.length; i++)
        {
            var col = this.col[i];
            $('#mtbl_header').append("<th id='lhead_"+i+"'>" + g_metadata[col.idx].field_name + "(" + col.mode + ")</th>");
            $("#lhead_"+i).width(col.width);
            $("#list_body_head").append("<th id='lbodyhead_"+i+"'> </th>");
            $("#lbodyhead_"+i).width(col.width);
        }
        
        
        this.update_view();
    },

    tree_reload: function()
    {
        this.list = {};
        this.ordered = [];
        this.num_items = 0;
        this.unordered = [];
        $('#mtbl').html("<tr id='list_body_head'></tr>");
        
        
        for(var thread_id in g_target_cct)
        {
            var thread = g_target_cct[thread_id];
            for(var key in thread.nodes)
            {
                var node = thread.nodes[key];
                this.update_item(thread, node);
            }
        }
        this.update_column();
    },

    update_size: function()
    {
        var mp = $('#mainpanel');
        $('#mtbl_scroll').height(mp.height());
        $('#mtbl_scroll').width(mp.width());
    },
    
    update_item: function(thread, node)
    {
        var thread_id = thread.id;
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
            $('#mtbl').append("<tr id='tbl_"+(this.num_items-1)+"'><td>0</td></tr>");
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
        accumurate(node_stock, node, thread);
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

    update_view: function()
    {
        this.merge_orderd();

        for(var i = 0; i < this.ordered.length && i < 50; i++)
        {
            var node_stock = this.ordered[i];
            var html = "<td>" + node_stock.name + "</td>";

            for(var j = 0; j < this.col.length; j++)
            {
                var col = this.col[j];
                html += "<td class='number'>";
                
                // get_profile_value_** は使わない
                //  -> accumurateで既につかってる
                if(col.mode == "all")
                    html += node_stock.all[col.idx];
                else if(col.mode == "self")
                    html += (node_stock.all[col.idx] - node_stock.cld[col.idx]);
                html += "</td>";
            }
            $('#tbl_' + i).html(html);
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
                Panels.list.update_item(updated[thread_id], node);
                
            }
        }
        this.update_view();
    },
    
    hide: function()
    {
        
    }
};



Panels.square = {
    
    init: function()
    {
        this.update_timer_enabled = false;
        if(!this.open_nodes)
            this.open_nodes = {};

        $("#mainpanel").html("<div id='sqv_ctrl'><div id='sq_depth_bar'></div></div><div id='sqv_parent'></div>");
        $("#sqv_ctrl")
            .height(30)
        ;
        $('#sq_depth_bar').slider({
            min: 2,
            max: 30,
            startValue: 5,
            slide: function(e, ui){
                if(!Panels.square.update_timer_enabled)
                {
                    Panels.square.update_timer_enabled = true;
                    setTimeout(function(){Panels.square.update_sq();}, 500);
                }
            }
        });

    },

    tree_reload: function()
    {
        this.update_sq();
    },

    update_sq: function()
    {
        $('#debug2').html("");
        $("#sqv_parent")
            .css("position", "relative")
            .width($("#mainpanel").width())
            .height($("#mainpanel").height() - 30)
            .html("")
        ;
        var thread = g_target_cct["0"];
        if(!thread)
            return;
        var root = thread.nodes[2];
        if(!root)
            return;

        this.draw({
            target_elem: $("#sqv_parent"),
            thread: thread,
            node: root,
            x: 0,
            y: 0,
            w: $("#sqv_parent").width(),
            h: $("#sqv_parent").height(),
            z: 0,
            depth: 0,
            color: 0,
            max_depth: $('#sq_depth_bar').slider("option", "value"),
        });

    },
    
    update_size: function()
    {
        
    },

    draw: function(data)
    {
        // $('#debug2').append("rect: " + data.x + ", " + data.y + ", " + data.w + ", " + data.h + ";<br />");
        if(data.depth >= data.max_depth)
            return;

        data.target_elem.append("<div class='sqv_sq' id='node_" + data.node.id + "'> </div>");
        $('#node_' + data.node.id)
            .css("left", data.x)
            .css("top", data.y)
            .css("z-index", data.z)
            .css("background-color", g_colors[data.color % g_colors.length])
            .width(data.w)
            .height(data.h)
            .html(data.node.name + " (" + data.node.id + ")")
        ;

        var allval = 0.0;
        var values = {};
        for(var key in data.node.cid)
        {
            var cnode = data.node.cid[key];
            values[cnode.id] = get_profile_value_all(data.thread, cnode, 0);
            allval += values[cnode.id];
        }
        if(get_profile_value_all(data.thread, data.node, 0) > allval)
            allval = get_profile_value_all(data.thread, data.node, 0);

        var psum = 0.0;
        var cur_color = data.color + 1;
        for(var key in data.node.cid)
        {
            var cnode = data.node.cid[key];
            var p = values[cnode.id] / allval;
            
            
            var next_data = {
                target_elem: data.target_elem,
                node: cnode,
                thread: data.thread,
                depth: data.depth,
                max_depth: data.max_depth,
                z: data.z + 1,
                color: cur_color
            };
            cur_color++;
            if(data.depth % 2 == 0)
            {
                next_data.x = data.x + data.w * psum;
                next_data.y = data.y;
                next_data.w = data.w * p;
                next_data.h = data.h;
            }
            else
            {
                next_data.x = data.x;
                next_data.y = data.y + data.h * psum;
                next_data.w = data.w;
                next_data.h = data.h * p;
            }
            next_data.depth += 1;
            next_data.x += 2;
            next_data.y += 2;
            next_data.w -= 4;
            next_data.h -= 4;
            if(next_data.w > 2 && next_data.h > 2)
                this.draw(next_data);
            psum += p;
            // assert(psum <= 1.1, "psum <= 1.1");
        }
    },

    

    update: function(updated)
    {
        this.update_sq();

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
    $('#mainpanel').height(win_h - 100 - 10);

    $('#modepanel').width(win_w - 200 - 10);
    $('#modepanel').height(100);


    $('#sidepanel').width(200);
    $('#sidepanel').height(win_h - 10);
    
    if(g_active_panel)
        Panels[g_active_panel].update_size();
}

function do_command(cmd)
{
    var cmd = cmd.split(" ");
    
    if(cmd[0] == "g" && cmd.length >= 4)
    {
        g_active_panel = null;
        $('#mainpanel').html("Loading...");
        
        fetch_table_data({
            threadid: "0",
            //nodeid: cmd[1],
            start_time: parseInt(cmd[1]),
            end_time: parseInt(cmd[2]),
            timewidth: parseInt(cmd[3]),
            nodes: cmd[4].split(','),
            //depth: parseInt(cmd[5]),
            cb: function(req){
                var html = "<table id='charting_table'><tr><td></td>";
                
                var nkeys = req.keys_idx.length;
                if(nkeys >= 10)
                {
                    alert("Too many keys");
                    nkeys = 10;
                }
                
                for(var i = 0; i < nkeys; i++)
                {
                    html += "<th scope='col'>" + req.keys_idx[i].name + "</th>";
                }
                html += "</tr>";
                
                for(var i = 0; i < req.data.length; i++)
                {
                    html += "<tr><th scope='row'>"+i+"</th>";
                    for(var j = 0; j < nkeys; j++)
                    {
                        var key = req.keys_idx[j].id;
                        if(req.data[i][key])
                            html += "<td>" + get_profile_value_all(req.thread_data[i], req.data[i][key], 0) + "</td>";
                        else
                            html += "<td>0</td>";
                    }
                    html += "</tr>";
                }
                html += "</table>";
                $('#mainpanel').html(html);
                $("#charting_table").visualize({
                    type: "line",
                    width: 700,
                    height: 500,
                    parseDirection: "y",
                    colors: [
                        '#be1e2d','#666699','#92d5ea','#ee8310','#8d10ee','#5a3b16','#26a4ed','#f45a90','#e9e744',
                        '#FF1e2d','#FF6699','#92FFea','#ee83FF','#FF10ee','#5a3bFF','#26FFed','#f4FF90','#00FFFF'
                        
                    ]
                });
                $("#charting_table").hide();
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
        req.thread_data = [];
        req.numkeys = 0;
        if(req.nodes)
        {
            req.node_map = {};
            for(var i = 0; i < req.nodes.length; i++)
            {
                req.node_map[req.nodes[i]] = true;
            }
        }
    }
    if(req.start_time >= req.end_time)
    {
        req.cb(req);
        return;
    }
    $.ajax({
        type: "POST",
        url: "/ds/tree/" + g_current_target + "/diff/" + (req.start_time - req.timewidth + 1) + "/" + req.start_time,
        data: {},
        dataType: "json",
        success: function(data){
            if(!(data.threads[req.threadid]))
            {
                alert("No Node");
                return;
            }
            var thread = data.threads[req.threadid];
            
            var node_id_map = {};
            for(var i = 0; i < thread.nodes.length; i++)
            {
                node_id_map[ thread.nodes[i].id ] = thread.nodes[i];
            }
            
            nth_parent_id = function(node, n)
            {
                for(var i = 0; i < n; i++)
                {
                    if(!node)
                        return null;
                    node = node_id_map[node.pid];
                }
                if(!node)
                    return null;
                return node.id;
            }
            
            
            var current = {};
            for(var nodeid in thread.nodes)
            {
                var node = thread.nodes[nodeid];
                //if(nth_parent_id(node, req.depth) == req.nodeid)
                if(node.id in req.node_map)
                {
                    current[node.id] = node;
                    if(!(node.id in req.keys))
                    {
                        req.keys[node.id] = {
                            idx: req.numkeys,
                            id: node.id,
                            name: node.name,
                        };
                        req.keys_idx.push(req.keys[node.id]);
                        req.numkeys++;
                    }
                }
            }
            thread.nodes = null;
            req.data.push(current);
            req.thread_data.push(thread);
            req.start_time += req.timewidth;
            fetch_table_data(req);
        },
        error: function(){
            alert("Data Error");
        }
    });
}


//////////////////////////////////////////////////////////////////////////
// Navigator
//////////////////////////////////////////////////////////////////////////


function update_nav()
{
    var panelname = $("#view_select").val();
    if(g_active_panel && Panels[g_active_panel])
    {
        Panels[g_active_panel].hide();
    }
    if(!panelname && !g_active_panel)
        g_active_panel = g_default_panel;
    else if(panelname)
        g_active_panel = panelname;
    Panels[g_active_panel].init();
    Panels[g_active_panel].tree_reload();



    var before_real_time = g_timenav.real_time;
    g_timenav.real_time = ($("#check_realtime").attr("checked") ? true : false);
    g_timenav.select_time = $("#timebar").slider("option", "value");
    g_timenav.select_width = $("#timewidthbar").slider("option", "value");

    $("#timebar_value").html(g_timenav.select_time);
    $("#timewidthbar_value").html(g_timenav.select_width);

    if(!before_real_time || !g_timenav.real_time)
    {
        g_target_cct = null;
    }
    g_need_update_counter = 1;




}


$(function(){
    setTimeout(update_ui, 500);
    fit_panel();
    $(window).resize(fit_panel);

    $('#timebar').slider({
        min: 0,
        max: 10,
        startValue: 0,
        slide: update_nav
    });
    $('#timewidthbar').slider({
        min: 1,
        max: 20,
        startValue: 1,
        slide: update_nav
    });
        
    $("#view_select").change(update_nav);
    
    $('#check_realtime').click(update_nav);
    
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
