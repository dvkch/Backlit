$(document).ready(function() {
  +function renderData(items, parentId, container) {
    for (var index in items) {
      var item = items[index];
      container.append("<tr data-tt-id='" + item.iden + "'" + (parentId?" data-tt-parent-id='" + parentId + "'":"") + "><td><span class='" + (item.type == "d"? "folder": "file") + "'>" + item.name + "</span></td><td>" + (item.type == "d"? "Folder": "File") + "</td><td>" + (item.size? item.size: "--") + "</td><td>" + item.owner + "</td><td>" + item.permi + "</td><td>" + item.creat + "</td><td>" + item.modif + "</td></tr>");
      renderData(item.subs, item.iden, container);
    }
  }(data, null, $("#example-advanced tbody"))

  $("#example-advanced").treetable({
    expandable: true
  });

  function actionOpen(t) {
    if (t.hasClass("leaf")) {
      if (t.find("span[class=folder]").length == 0) {
        var win = window.open ('/open/' + encodeURI(t.data("ttId")), '_blank');
        if (!win) {
          showMessage("Please allow pop-ups for this site")
        }
      }
    } else {
      if (!t.hasClass("expanded")) {
        $("#example-advanced").treetable("expandNode", t.data("ttId"));
      } else {  
        $("#example-advanced").treetable("collapseNode", t.data("ttId"));
      }
    }
  }

  function actionDelete(t) {
    $.ajax('/delete/' + encodeURI(t.data("ttId")))
      .done(function(r) {
        if (r == "success") {
          $("#example-advanced").treetable("collapseNode", t.data("ttId"));
          t.remove();
        } else {
          showMessage(r);
        }
      })
      .fail(function(r) {
        showMessage("[" + r.status + "] " + r.statusText + " " + r.responseText);
      });
  }

  // Message display
  var messageTimeout;
    $("#message-modal").on('hidden.bs.modal', function (e) {
      clearTimeout(messageTimeout);
  });

  showMessage = function(message) {
    console.log(message);
    $("#message-modal .modal-body").html(message);
    $("#message-modal").modal({show: true, keyboard: true});
    messageTimeout = setTimeout(function(){
      $("#message-modal").modal("hide");
    }, 3500);
  };

  // Highlight selected row
  $("#example-advanced tbody").on("click", "tr", function(evt) {
    $(".selected").not(this).removeClass("selected");
    $(this).addClass("selected");
    if (evt.target.tagName !== "A") {
      actionOpen($(this));
    };
  });

  +function bindContexMenu() {
    var selItem;
    var $contextMenu = $("#contextMenu");
    $("#example-advanced tbody").on("contextmenu", "tr", function(e) {
      $(".selected").not(this).removeClass("selected");
      $(this).addClass("selected");
      selItem = $(this)
      $contextMenu.css({
        display: "block",
        left: e.pageX,
        top: e.pageY
      });
      return false;
    });
    
    $contextMenu.on("click", "a", function(evt) {
      switch ($(evt.target).data("action")) {
        case "open":
          actionOpen(selItem);
          break;
        case "delete":
          actionDelete(selItem);
          break;
      };
      $contextMenu.hide();
    });
  }();
  
  // $.contextMenu({
  //   selector: '#example-advanced tbody', 
  //   callback: function(key, options) {
  //     var m = "clicked: " + key;
  //     window.console && console.log(m) || alert(m); 
  //   },
  //   items: {
  //     "open": {name: "Open", icon: "edit"},
  //     "delete": {name: "Delete", icon: ""}
  //   }
  // });

});