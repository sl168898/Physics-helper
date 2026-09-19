// Methods merged into SkyUI 6.11's BarterMenu by tools/build_ui.py.
// Original SkyUI code and artwork retain their upstream permissions.
class BalancedBarterExtensions
{
   var _bbAPI;
   var _bbPanel;
   var _bbState;
   var _bbPage = 0;
   var _bbSubMenu = false;
   var _bbHeight = 186;
   var _bbPlayerInfo;
   var _bbVendorName;

   function InitExtensions()
   {
      super.InitExtensions();
      gfx.io.GameDelegate.addCallBack("SetBarterMultipliers",this,"SetBarterMultipliers");
      this.itemCard.addEventListener("sliderChange",this,"onQuantitySliderChange");
      this.inventoryLists.tabBarIconArt = this._tabBarIconArt;
      this.inventoryLists.categoryList.iconArt = this._categoryListIconArt;
      this._bbAPI = skse.plugins.BalancedBarter;
      this._bbState = {entries:[],buy:0,sell:0,net:0,ready:false,busy:false,message:"Select items on both sides, then confirm"};
      this.BBCreatePanel();
      this.updateDynamicListHeight();
      if(this._bbAPI == undefined)
      {
         this._bbState.message = "BalancedBarter.dll did not load. Check SKSE and Skyrim 1.6.1170.";
      }
      this.BBDraw();
   }
   function UpdatePlayerInfo(a_playerGold, a_vendorGold, a_vendorName, a_playerUpdateObj)
   {
      this._vendorGold = a_vendorGold;
      this._playerGold = a_playerGold;
      this._bbPlayerInfo = a_playerUpdateObj;
      this._bbVendorName = a_vendorName;
      this.bottomBar.updateBarterInfo(a_playerUpdateObj,this.itemCard.itemInfo,a_playerGold,a_vendorGold,a_vendorName);
      if(this._bbAPI != undefined && this._bbState.busy != true)
      {
         this.BBResult(this._bbAPI.Status(this._playerGold,this._vendorGold));
      }
   }
   function onItemSelect(event)
   {
      if(this._bbAPI == undefined || this._bbState.busy || this._bbSubMenu || event.entry == undefined)
      {
         return;
      }
      // The native adapter checks eligibility. A cash shortage must not stop
      // an item from entering a balanced offer.
      if(this._quantityMinCount < 1 || event.entry.count < this._quantityMinCount)
      {
         this.doTransaction(1);
      }
      else
      {
         this.itemCard.ShowQuantityMenu(event.entry.count);
      }
   }
   function onQuantityMenuSelect(event)
   {
      this.doTransaction(event.amount);
   }
   function onQuantitySliderChange(event)
   {
      // Pending totals are shown in the offer panel, not as an immediate sale.
      this.bottomBar.updateBarterPriceInfo(this._playerGold,this._vendorGold);
   }
   function onItemCardSubMenuAction(event)
   {
      this._bbSubMenu = event.opening;
      super.onItemCardSubMenuAction(event);
      this.bottomBar.updateBarterPriceInfo(this._playerGold,this._vendorGold);
   }
   function onTransactionConfirm()
   {
      // All exchanges use the explicit offer confirmation controls.
   }
   function doTransaction(a_amount)
   {
      if(this._bbAPI == undefined || this._bbState.busy)
      {
         return;
      }
      this.BBResult(this._bbAPI.Queue(a_amount,this.itemCard.itemInfo.value,this.isViewingVendorItems(),this._playerGold,this._vendorGold));
   }
   function onExitButtonPress()
   {
      if(this._bbState.busy)
      {
         return;
      }
      if(this._bbAPI != undefined)
      {
         this._bbAPI.Close();
      }
      gfx.io.GameDelegate.call("CloseMenu",[]);
   }
   function handleInput(details, pathToFocus)
   {
      if(this._bbState.busy)
      {
         return true;
      }
      if(!this._bbSubMenu && this.bFadedIn && Shared.GlobalFunc.IsKeyPressed(details))
      {
         var key = details.skseKeycode;
         if((this._platform == 0 && key == 28 && Key.isDown(17)) || (this._platform != 0 && key == 278))
         {
            this.BBConfirm();
            return true;
         }
         if((this._platform == 0 && key == 14 && Key.isDown(17)) || (this._platform != 0 && key == 279))
         {
            this.BBClear();
            return true;
         }
         if(key == 201 || (this._platform != 0 && key == 280))
         {
            this.BBPage(-1);
            return true;
         }
         if(key == 209 || (this._platform != 0 && key == 281))
         {
            this.BBPage(1);
            return true;
         }
      }
      return super.handleInput(details,pathToFocus);
   }
   function updateDynamicListHeight()
   {
      super.updateDynamicListHeight();
      if(this._bbPanel != undefined)
      {
         this.inventoryLists.itemList.listHeight = Math.max(100,this.inventoryLists.itemList.listHeight - this._bbHeight - 10);
         this.inventoryLists.itemList.requestUpdate();
         this.BBLayout();
      }
   }
   function updateBottomBar(a_bSelected)
   {
      this.navPanel.clearButtons();
      if(a_bSelected)
      {
         this.navPanel.addButton({text:"Add to offer",controls:skyui.defines.Input.Activate});
      }
      else
      {
         this.navPanel.addButton({text:"$Exit",controls:this._cancelControls});
         this.navPanel.addButton({text:"$Search",controls:this._searchControls});
         this.navPanel.addButton({text:"$Switch Tab",controls:this._switchControls});
      }
      this.navPanel.updateButtons(true);
   }
   function BBResult(value)
   {
      if(value == undefined || value.entries == undefined)
      {
         return;
      }
      this._bbState = value;
      this._playerGold = value.playerGold;
      this._vendorGold = value.vendorGold;
      if(this._bbPlayerInfo != undefined)
      {
         this.bottomBar.updateBarterInfo(this._bbPlayerInfo,this.itemCard.itemInfo,this._playerGold,this._vendorGold,this._bbVendorName);
      }
      this.BBDraw();
   }
   function BBConfirm()
   {
      if(this._bbAPI != undefined && !this._bbState.busy && !this._bbSubMenu)
      {
         this.BBResult(this._bbAPI.Commit(this._playerGold,this._vendorGold));
      }
   }
   function BBClear()
   {
      if(this._bbAPI != undefined && !this._bbState.busy)
      {
         this._bbPage = 0;
         this.BBResult(this._bbAPI.Clear(this._playerGold,this._vendorGold));
      }
   }
   function BBRemove(id)
   {
      if(this._bbAPI != undefined && !this._bbState.busy)
      {
         this.BBResult(this._bbAPI.Remove(id,this._playerGold,this._vendorGold));
      }
   }
   function BBPage(direction)
   {
      this._bbPage += direction;
      this.BBDraw();
   }
   function BBText(parent, name, x, y, width, text, size, color)
   {
      parent.createTextField(name,parent.getNextHighestDepth(),x,y,width,21);
      var field = parent[name];
      field.selectable = false;
      field.multiline = false;
      field.wordWrap = false;
      field.text = text;
      var format = new TextFormat("$EverywhereMediumFont",size,color);
      field.setTextFormat(format);
      field.setNewTextFormat(format);
      return field;
   }
   function BBButton(name, x, y, width, label, action, id)
   {
      var button = this._bbPanel.createEmptyMovieClip(name,this._bbPanel.getNextHighestDepth());
      button._x = x;
      button._y = y;
      button.beginFill(0x31302c,95);
      button.moveTo(0,0);
      button.lineTo(width,0);
      button.lineTo(width,21);
      button.lineTo(0,21);
      button.endFill();
      this.BBText(button,"label",4,0,width - 8,label,12,0xeee9dc);
      button.owner = this;
      button.action = action;
      button.itemId = id;
      button.onRelease = function()
      {
         this.owner[this.action](this.itemId);
      };
      button.onRollOver = function() { this._alpha = 75; };
      button.onRollOut = function() { this._alpha = 100; };
      return button;
   }
   function BBCreatePanel()
   {
      this._bbPanel = this.createEmptyMovieClip("BalancedBarterPanel",this.getNextHighestDepth());
      this.BBLayout();
   }
   function BBLayout()
   {
      if(this._bbPanel == undefined)
      {
         return;
      }
      var bounds = this.inventoryLists.getContentBounds();
      this._bbPanel._x = this.inventoryLists._x + bounds[0] + 20;
      this._bbPanel._y = this.bottomBar._y - this._bbHeight - 5;
      this._bbPanel.panelWidth = Math.max(430,bounds[2] - 28);
      this.BBDraw();
   }
   function BBDraw()
   {
      if(this._bbPanel == undefined || this._bbState == undefined)
      {
         return;
      }
      var panel = this._bbPanel;
      for(var key in panel)
      {
         if(typeof(panel[key]) == "movieclip") { panel[key].removeMovieClip(); }
         else if(panel[key] instanceof TextField) { panel[key].removeTextField(); }
      }
      panel.clear();
      var width = panel.panelWidth;
      var column = (width - 24) / 2;
      panel.lineStyle(1,0xa99a72,80);
      panel.beginFill(0x131516,94);
      panel.moveTo(0,0);
      panel.lineTo(width,0);
      panel.lineTo(width,this._bbHeight);
      panel.lineTo(0,this._bbHeight);
      panel.lineTo(0,0);
      panel.endFill();
      this.BBText(panel,"giveTitle",8,4,column,"YOU OFFER  " + this._bbState.sell + " gold",14,0xdfcd9a);
      this.BBText(panel,"takeTitle",column + 16,4,column,"YOU RECEIVE  " + this._bbState.buy + " gold",14,0xdfcd9a);
      var give = [];
      var take = [];
      for(var i = 0; i < this._bbState.entries.length; i++)
      {
         var entry = this._bbState.entries[i];
         if(entry.buy) { take.push(entry); } else { give.push(entry); }
      }
      var pages = Math.max(1,Math.ceil(Math.max(give.length,take.length) / 4));
      this._bbPage = Math.max(0,Math.min(pages - 1,this._bbPage));
      for(var side = 0; side < 2; side++)
      {
         var rows = side == 0 ? give : take;
         for(var row = 0; row < 4; row++)
         {
            var item = rows[this._bbPage * 4 + row];
            if(item != undefined)
            {
               var label = "- " + item.count + "x " + item.name + "  (" + item.count * item.price + ")";
               this.BBButton("row" + side + "_" + row,8 + side * (column + 8),26 + row * 21,column - 1,label,"BBRemove",item.id);
            }
         }
      }
      var net = this._bbState.net;
      var balance = net == 0 ? "Even exchange - no gold changes hands" : (net > 0 ? "You pay " + net + " gold" : "You receive " + -net + " gold");
      this.BBText(panel,"balance",8,112,width - 16,balance,14,this._bbState.ready ? 0xb9dba7 : 0xe0b7a7);
      this.BBText(panel,"message",8,132,width - 16,this._bbState.message,11,0xcacaca);
      var confirm = this._platform == 0 ? "[Ctrl+Enter] Exchange" : "[X] Exchange";
      var clear = this._platform == 0 ? "[Ctrl+Backspace] Clear" : "[Y] Clear";
      this.BBButton("confirm",8,157,column - 4,confirm,"BBConfirm",0);
      this.BBButton("clearOffer",column + 12,157,column - 75,clear,"BBClear",0);
      this.BBButton("previous",width - 68,157,27,"<","BBPage",-1);
      this.BBButton("next",width - 36,157,27,">","BBPage",1);
      if(pages > 1)
      {
         this.BBText(panel,"pages",width - 83,112,77,(this._bbPage + 1) + "/" + pages,11,0xcacaca);
      }
      panel.confirm._alpha = this._bbState.ready ? 100 : 45;
   }
}
