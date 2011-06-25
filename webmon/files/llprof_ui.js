
function update_ui()
{
    $.ajax({
        type: "POST",
        url: "/ds/list",
        dataType: "json",
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
        }
        var target_line = $("#target_item_" + data[i].id);
        
        target_line.children(".name").html(data[i].name);
        target_line.children(".host").html(data[i].host);
    }
    setTimeout(update_ui, 500);
}

function fit_panel()
{
    var win_h = $(window).height();
    var win_w = $(window).width();
    $('#mainpanel').width(win_w - 200 - 10);
    $('#mainpanel').height(win_h - 10);
    $('#sidepanel').width(200);
    $('#sidepanel').height(win_h - 10);
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
