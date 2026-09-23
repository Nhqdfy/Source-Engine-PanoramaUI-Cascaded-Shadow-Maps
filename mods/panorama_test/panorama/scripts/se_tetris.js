"use strict";
// SE port (2026-09-23): 地图内俄罗斯方块。
//
// 设计要点：
//   * 棋盘 10x20，格子用 200 个 Panel（每格 24x24），逐帧只改 background-color —— 这个移植没有
//     canvas 那套，纯 Panel 最稳。
//   * 键盘用 panorama 的 JS 键绑定 $.RegisterKeyBind（CS:GO 内容 teamselectmenu.js 就是这条路），
//     **每个键单独注册一个处理器** —— 这个 API 的回调不带"按了哪个键"的参数。
//   * 重力用 $.Schedule 定时下落；暂停/重开/关闭都在面板内处理。
//   * 关掉自己：走引擎命令 se_tetris 0（engine/panoramaenginehandler.cpp 建的托管视图）。
(function () {
	var COLS = 10;
	var ROWS = 20;
	var CELL = 24;

	// piece = { color, rots: [ 4 个旋转，每个是 4 个 [x,y] 格子 ] }
	var PIECES = [
		{ c: "#4fc3f7ff", rots: [ [[0,1],[1,1],[2,1],[1,0]], [[1,0],[1,1],[1,2],[0,1]], [[0,1],[1,1],[2,1],[1,2]], [[1,0],[1,1],[1,2],[2,1]] ] },	// T
		{ c: "#81c784ff", rots: [ [[0,0],[0,1],[1,1],[1,2]], [[0,1],[1,1],[1,0],[2,0]], [[0,0],[0,1],[1,1],[1,2]], [[0,1],[1,1],[1,0],[2,0]] ] },	// S
		{ c: "#e57373ff", rots: [ [[1,0],[1,1],[0,1],[0,2]], [[0,0],[1,0],[1,1],[2,1]], [[1,0],[1,1],[0,1],[0,2]], [[0,0],[1,0],[1,1],[2,1]] ] },	// Z
		{ c: "#ffb74dff", rots: [ [[0,0],[1,0],[0,1],[1,1]], [[0,0],[1,0],[0,1],[1,1]], [[0,0],[1,0],[0,1],[1,1]], [[0,0],[1,0],[0,1],[1,1]] ] },	// O
		{ c: "#64b5f6ff", rots: [ [[0,1],[1,1],[2,1],[3,1]], [[2,0],[2,1],[2,2],[2,3]], [[0,2],[1,2],[2,2],[3,2]], [[1,0],[1,1],[1,2],[1,3]] ] },	// I
		{ c: "#9575cdff", rots: [ [[0,1],[1,1],[2,1],[2,0]], [[1,0],[1,1],[1,2],[2,2]], [[0,2],[0,1],[1,1],[2,1]], [[0,0],[1,0],[1,1],[1,2]] ] },	// J
		{ c: "#ffd54fff", rots: [ [[0,0],[0,1],[1,1],[2,1]], [[1,0],[1,1],[1,2],[2,0]], [[0,1],[1,1],[2,1],[2,2]], [[0,2],[1,0],[1,1],[1,2]] ] },	// L
	];

	var g_Board = [];			// ROWS x COLS，null 或颜色字符串
	var g_Cells = [];			// 棋盘 Panel
	var g_NextCells = [];		// 预览 Panel（4x4）
	var g_Cur = null;			// { p, rot, x, y }
	var g_Next = null;
	var g_Score = 0, g_Lines = 0, g_Level = 1;
	var g_Paused = false, g_Over = false;
	var g_FallMs = 700;
	var g_LastFall = 0;
	var g_TickHandle = null;

	function emptyBoard() {
		var b = [];
		for (var y = 0; y < ROWS; ++y) {
			var row = [];
			for (var x = 0; x < COLS; ++x) { row.push(""); }
			b.push(row);
		}
		return b;
	}

	function randPiece() {
		return { p: PIECES[Math.floor(Math.random() * PIECES.length)], rot: 0 };
	}

	function cellsOf(piece) {
		return piece.p.rots[piece.rot];
	}

	function collides(piece, px, py, rot) {
		var rotIdx = (rot === undefined) ? piece.rot : rot;
		var cells = piece.p.rots[rotIdx];
		for (var i = 0; i < cells.length; ++i) {
			var x = px + cells[i][0];
			var y = py + cells[i][1];
			if (x < 0 || x >= COLS || y >= ROWS) { return true; }
			if (y >= 0 && g_Board[y][x]) { return true; }
		}
		return false;
	}

	function spawn() {
		g_Cur = { p: g_Next.p, rot: 0, x: 3, y: 0 };
		g_Next = randPiece();
		if (collides(g_Cur, g_Cur.x, g_Cur.y)) {
			g_Over = true;
			setState("游戏结束 —— 按 R 重开");
		}
		drawNext();
	}

	function lockPiece() {
		var cells = cellsOf(g_Cur);
		for (var i = 0; i < cells.length; ++i) {
			var x = g_Cur.x + cells[i][0];
			var y = g_Cur.y + cells[i][1];
			if (y >= 0 && y < ROWS && x >= 0 && x < COLS) { g_Board[y][x] = g_Cur.p.c; }
		}
		clearLines();
		spawn();
	}

	function clearLines() {
		var cleared = 0;
		for (var y = ROWS - 1; y >= 0; --y) {
			var full = true;
			for (var x = 0; x < COLS; ++x) { if (!g_Board[y][x]) { full = false; break; } }
			if (!full) { continue; }
			g_Board.splice(y, 1);
			g_Board.unshift(new Array(COLS).join(",").split(",").map(function () { return ""; }));
			++cleared;
			++y;		// 这一行被移走了，同一 y 还要再看一次
		}
		if (!cleared) { return; }

		g_Lines += cleared;
		g_Score += [0, 100, 300, 500, 800][cleared] * g_Level;
		g_Level = 1 + Math.floor(g_Lines / 10);
		g_FallMs = Math.max(120, 700 - (g_Level - 1) * 60);
		refreshStats();
	}

	function move(dx) {
		if (g_Paused || g_Over || !g_Cur) { return; }
		if (!collides(g_Cur, g_Cur.x + dx, g_Cur.y)) { g_Cur.x += dx; draw(); }
	}

	function rotate() {
		if (g_Paused || g_Over || !g_Cur) { return; }
		var nextRot = (g_Cur.rot + 1) % 4;
		// 简单的踢墙：原位不行就左/右挪一格再试
		var offsets = [0, -1, 1, -2, 2];
		for (var i = 0; i < offsets.length; ++i) {
			if (!collides(g_Cur, g_Cur.x + offsets[i], g_Cur.y, nextRot)) {
				g_Cur.x += offsets[i];
				g_Cur.rot = nextRot;
				draw();
				return;
			}
		}
	}

	function softDrop() {
		if (g_Paused || g_Over || !g_Cur) { return; }
		if (!collides(g_Cur, g_Cur.x, g_Cur.y + 1)) { g_Cur.y += 1; draw(); }
		else { lockPiece(); draw(); }
	}

	function hardDrop() {
		if (g_Paused || g_Over || !g_Cur) { return; }
		while (!collides(g_Cur, g_Cur.x, g_Cur.y + 1)) { g_Cur.y += 1; }
		lockPiece();
		draw();
	}

	function setState(s) {
		var el = $.GetContextPanel().FindChildTraverse("SeTetrisState");
		if (el) { el.text = s || ""; }
	}

	function refreshStats() {
		var root = $.GetContextPanel();
		var a = root.FindChildTraverse("SeTetrisScore");
		if (a) { a.text = "分数 " + g_Score; }
		var b = root.FindChildTraverse("SeTetrisLines");
		if (b) { b.text = "消行 " + g_Lines; }
		var c = root.FindChildTraverse("SeTetrisLevel");
		if (c) { c.text = "等级 " + g_Level; }
	}

	function buildBoard() {
		var board = $.GetContextPanel().FindChildTraverse("SeTetrisBoard");
		if (!board) { return; }
		for (var y = 0; y < ROWS; ++y) {
			g_Cells.push([]);
			for (var x = 0; x < COLS; ++x) {
				var p = $.CreatePanel("Panel", board, "SeT" + x + "_" + y);
				p.style.width = (CELL - 2) + "px";
				p.style.height = (CELL - 2) + "px";
				p.style.margin = "1px";
				p.style.backgroundColor = "#12171cff";
				p.hittest = false;
				g_Cells[y].push(p);
			}
		}

		var next = $.GetContextPanel().FindChildTraverse("SeTetrisNext");
		if (next) {
			for (var i = 0; i < 16; ++i) {
				var q = $.CreatePanel("Panel", next, "SeTN" + i);
				q.style.width = (CELL + 2) + "px";
				q.style.height = (CELL + 2) + "px";
				q.style.margin = "1px";
				q.style.backgroundColor = "#12171cff";
				q.hittest = false;
				g_NextCells.push(q);
			}
		}
	}

	function draw() {
		for (var y = 0; y < ROWS; ++y) {
			for (var x = 0; x < COLS; ++x) {
				g_Cells[y][x].style.backgroundColor = g_Board[y][x] || "#12171cff";
			}
		}
		if (!g_Cur) { return; }
		var cells = cellsOf(g_Cur);
		for (var i = 0; i < cells.length; ++i) {
			var cx = g_Cur.x + cells[i][0];
			var cy = g_Cur.y + cells[i][1];
			if (cy >= 0 && cy < ROWS && cx >= 0 && cx < COLS) {
				g_Cells[cy][cx].style.backgroundColor = g_Cur.p.c;
			}
		}
	}

	function drawNext() {
		for (var i = 0; i < g_NextCells.length; ++i) { g_NextCells[i].style.backgroundColor = "#12171cff"; }
		if (!g_Next) { return; }
		var cells = g_Next.p.rots[0];
		// 预览用 4x4 里居中画一遍（偏移 1，避免贴边）
		for (var k = 0; k < cells.length; ++k) {
			var x = cells[k][0] + 1;
			var y = cells[k][1] + 1;
			if (x < 0 || x > 3 || y < 0 || y > 3) { continue; }
			g_NextCells[y * 4 + x].style.backgroundColor = g_Next.p.c;
		}
	}

	function tick() {
		if (!g_Paused && !g_Over) {
			var now = Date.now();
			if (!g_LastFall || (now - g_LastFall) >= g_FallMs) {
				g_LastFall = now;
				if (!collides(g_Cur, g_Cur.x, g_Cur.y + 1)) { g_Cur.y += 1; }
				else { lockPiece(); }
				draw();
			}
		}
		$.Schedule(0.05, tick);
	}

	function restart() {
		g_Board = emptyBoard();
		g_Score = 0; g_Lines = 0; g_Level = 1; g_FallMs = 700;
		g_Paused = false; g_Over = false; g_LastFall = 0;
		g_Next = randPiece();
		spawn();
		refreshStats();
		setState("");
		draw();
	}

	function bindKeys(root) {
		// 每个键单独注册（RegisterKeyBind 的回调不带键名参数）
		$.RegisterKeyBind(root, "key_left", function () { move(-1); });
		$.RegisterKeyBind(root, "key_right", function () { move(1); });
		$.RegisterKeyBind(root, "key_up", rotate);
		$.RegisterKeyBind(root, "key_down", softDrop);
		$.RegisterKeyBind(root, "key_space", hardDrop);
		$.RegisterKeyBind(root, "key_p", function () {
			g_Paused = !g_Paused;
			setState(g_Paused ? "已暂停（P 继续）" : "");
		});
		$.RegisterKeyBind(root, "key_r", restart);
		$.RegisterKeyBind(root, "key_escape", function () {
			// 关掉整个视图（和打开对称）：走引擎命令；顺手先把自我隐藏当作兜底
			try { GameInterfaceAPI.ConsoleCommand("se_tetris 0"); } catch (e) { }
			var r = $.GetContextPanel();
			if (r) { r.style.visibility = "collapse"; }
		});
	}

	function onLoad() {
		var root = $.GetContextPanel();
		if (!root) { return; }

		buildBoard();
		bindKeys(root);
		restart();
		tick();
		$.Msg("[SE port] 俄罗斯方块: 已启动（10x20，P 暂停 / R 重开 / Esc 关闭）");
	}

	$.Schedule(0.0, onLoad);
})();
