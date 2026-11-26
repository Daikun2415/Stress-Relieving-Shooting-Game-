# include <Siv3D.hpp>

struct Target
{
	Circle circle{ 0, 0, 30 };
	Stopwatch timer{ StartImmediately::Yes };
	bool isBad = false; // trueなら青い的
};

struct ExplosionParticle
{
	Vec2 pos{ 0,0 };
	Vec2 velocity{ 0,0 };
	ColorF color{ Palette::White };

	double radius = 4.0;
	double life = 0.6;
};

struct ScorePopup
{
	Vec2 pos;

	String text;

	ColorF color;

	double life = 1.0;
};

Array<ScorePopup> popups;

Array<ExplosionParticle> effects;

void Main()
{
	Audio titleBGM{ U"source/maou_bgm_cyber44.mp3" };
	Audio resultBGM{ U"source/休み時間.mp3" };
	Audio gameBGM{ U"source/戦闘系04.mp3" };
	Audio shotSE{ U"source/拳銃を撃つ.mp3" };
	Audio hitSE{ U"source/ガラスが割れる2.mp3" };
	Audio missSE{ U"source/クイズ不正解1.mp3" };
	Audio feverSE{ U"source/シャキーン1.mp3" };
	Texture background{ U"source/廃墟ビルの部屋（明）.jpg" };

	Window::Resize(1920, 1080);
	Window::SetFullscreen(true);
	Scene::SetResizeMode(ResizeMode::Keep);

	Texture cursorTexture{ U"source/Gun.png" };

	const Array<String> emojis = { U"🐘", U"🐧", U"🐐", U"🐤" };

	Array<MultiPolygon> polygons;
	Array<Texture> textures;

	for (const auto& emoji : emojis)
	{
		polygons << Emoji::CreateImage(emoji)
			.alphaToPolygonsCentered()
			.simplified(2.0);

		textures << Texture{ Emoji{ emoji } };
	}

	constexpr double StepTime = (1.0 / 200.0);

	double accumulatedTime = 0.0;

	P2World world;

	Array<P2Body> bodies;

	HashTable<P2BodyID, size_t> table;

	size_t index = Random(polygons.size() - 1);

	bool facingRight = true;

	int32 score = 0;

	//フォント
	Font font{ FontMethod::MSDF, 40 };

	// ------------------------
	// 的
	// ------------------------
	Array<Target> targets;

	// 的生成タイマー
	double targetSpawnTimer = 0.0;

	// 制限時間
	double timeLimit = 30.0;

	// ゲーム終了フラグ
	enum class GameState
	{
		Title,
		Playing,
		Result
	};

	GameState gameState = GameState::Title;
	GameState previousState = gameState;

	int32 feverGauge = 0;

	bool isFever = false;

	double feverTime = 0.0;

	double shakeTime = 0.0;

	bool showHelp = false;

	constexpr int32 FeverNeed = 10;
	constexpr double FeverDuration = 5.0;

	for (auto* bgm : { &titleBGM, &gameBGM, &resultBGM })
	{
		bgm->setVolume(0.03);
		bgm->setLoop(true);
	}

	titleBGM.play();

	auto CreateExplosion =
		[&](const Vec2& pos, const ColorF& color)
		{
			for (int i = 0; i < 35; i++)
			{
				const double angle = Random(0.0, Math::TwoPi);
				const double speed = Random(120.0, 420.0);

				ExplosionParticle e;

				e.pos = pos;
				e.velocity = Circular(speed, angle);
				e.color = color;
				e.radius = Random(3.0, 7.0);
				e.life = Random(0.35, 0.7);

				effects << e;
			}
		};

	while (System::Update())
	{
		// シェイク更新
		if (shakeTime > 0)
		{
			shakeTime -= Scene::DeltaTime();

			if (shakeTime < 0)
			{
				shakeTime = 0;
			}
		}

		for (auto it = effects.begin(); it != effects.end();)
		{
			it->life -= Scene::DeltaTime();

			it->pos += it->velocity * Scene::DeltaTime();

			it->velocity *= 0.93;

			it->radius += 20 * Scene::DeltaTime();

			if (it->life <= 0)
			{
				it = effects.erase(it);
			}
			else
			{
				++it;
			}
		}

		for (auto it = popups.begin(); it != popups.end();)
		{
			it->life -= Scene::DeltaTime();

			it->pos.y -= 60 * Scene::DeltaTime();

			if (it->life <= 0)
			{
				it = popups.erase(it);
			}
			else
			{
				++it;
			}
		}


		Vec2 shakeOffset{ 0, 0 };

		if (shakeTime > 0)
		{
			const double power = 12.0 * (shakeTime / 0.3);

			shakeOffset =
			{
				Random(-power, power),
				Random(-power, power)
			};
		}

		{
			const Transformer2D transformer{Mat3x2::Translate(shakeOffset)};
			background.resized(Scene::Size()).draw();
		}
		

		

		if (gameState != previousState)
		{

			titleBGM.stop();
			gameBGM.stop();
			resultBGM.stop();

			switch (gameState)
			{
			case GameState::Title:
				titleBGM.setVolume(0.03);
				titleBGM.play();
				break;

			case GameState::Playing:
				gameBGM.setVolume(0.03);
				gameBGM.play();
				break;

			case GameState::Result:
				resultBGM.setVolume(0.03);
				resultBGM.play();
				break;
			}

			previousState = gameState;
		}

		if (gameState == GameState::Title)
		{
			// タイトル背景
			RoundRect{ Arg::center(Scene::Center().x, 300), 700, 80, 20 }
			.draw(ColorF{ 0.0, 0.7 });   // 黒、透明度70%

			font(U"ストレス発散シューティングゲーム")
				.drawAt(Scene::Center().x, 300, Palette::Yellow);

			// START背景
			RoundRect{ Arg::center(Scene::Center().x, 500), 450, 60, 15 }
			.draw(ColorF{ 0.0, 0.6 });

			font(U"Press SPACE to Start")
				.drawAt(Scene::Center().x, 500, Palette::White);

			// HowTo背景
			RoundRect{ Arg::center(Scene::Center().x, 600), 450, 60, 15 }
			.draw(ColorF{ 0.0, 0.6 });

			font(U"Press H : How To Play")
				.drawAt(Scene::Center().x, 600, Palette::Skyblue);

			if (KeySpace.down())
			{
				gameState = GameState::Playing;
			}

			if (KeyH.down())
			{
				showHelp = !showHelp;
			}

			if (showHelp)
			{
				// 画面全体を暗くする
				Scene::Rect().draw(ColorF{ 0, 0, 0, 0.5 });

				// 説明ウィンドウ
				RoundRect{
					Arg::center(Scene::Center().x, Scene::Center().y),
					1000, 500, 20
				}.draw(ColorF{ 0.1, 0.1, 0.1, 0.9 });

				RoundRect{
					Arg::center(Scene::Center().x, Scene::Center().y),
					1000, 500, 20
				}.drawFrame(3, Palette::Yellow);

				font(U"【遊び方】")
					.drawAt(Scene::Center().x, 220, Palette::Yellow);

				font(U"マウスのカーソル操作：銃を動かす")
					.drawAt(Scene::Center().x, 270, Palette::White);

				font(U"左クリック : 発射")
					.drawAt(Scene::Center().x, 320, Palette::White);

				font(U"右クリック : 銃の向きを変更")
					.drawAt(Scene::Center().x, 370, Palette::White);

				font(U"赤い的を撃つと +1点")
					.drawAt(Scene::Center().x, 430, Palette::Red);

				font(U"青い的を撃つと -1点")
					.drawAt(Scene::Center().x, 490, Palette::Blue);

				font(U"赤い的を10個当てると FEVER!　的壊し放題だ！")
					.drawAt(Scene::Center().x, 550, Palette::Gold);

				font(U"めざせ！ハイスコア！")
					.drawAt(Scene::Center().x, 610, Palette::Orange);

				font(U"Press H to Close")
					.drawAt(Scene::Center().x, 710, Palette::Skyblue);
			}
			else
			{
				if (KeySpace.down())
				{
					gameState = GameState::Playing;
				}
			}
			continue;
		}

		accumulatedTime += Scene::DeltaTime();

		// ------------------------
		// 制限時間を減らす
		// ------------------------
		if (gameState == GameState::Playing)
		{
			timeLimit -= Scene::DeltaTime();

			if (timeLimit <= 0)
			{
				timeLimit = 0;
				gameState = GameState::Result;
			}
		}

		while (StepTime <= accumulatedTime)
		{
			world.update(StepTime);
			accumulatedTime -= StepTime;
		}

		// ------------------------
		// 的を一定時間ごとに生成
		// ------------------------
		targetSpawnTimer += Scene::DeltaTime();

		double spawnInterval = 0.4;

		if (isFever)
		{
			spawnInterval = 0.15;
		}

		if (gameState == GameState::Playing && targetSpawnTimer >= spawnInterval)
		{
			targetSpawnTimer = 0.0;

			Target t;

			t.circle = Circle{
				Random(200, 1100),
				Random(100, 600),
				30
			};

			t.isBad = (Random(1, 100) <= 20);

			targets << t;
		}

		if (isFever)
		{
			feverTime -= Scene::DeltaTime();

			if (feverTime <= 0)
			{
				isFever = false;
				feverTime = 0.0;
			}
		}

		// 右クリックで向き反転
		if (MouseR.down())
		{
			facingRight = !facingRight;
		}

		// 左クリックで発射
		if (gameState == GameState::Playing && MouseL.down())
		{
			shotSE.playOneShot(0.1);

			Vec2 spawnPos = Cursor::PosF()
				+ Vec2{ facingRight ? 150 : -150, -70 };

			bodies << world.createPolygons(
				P2Dynamic,
				spawnPos,
				polygons[index].scaled(0.5),
				P2Material{ 0.1, 0.0, 1.0 }
			);

			double vx = facingRight ? 5000 : -5000;

			bodies.back().setVelocity(Vec2{ vx, -5.0 });

			table.emplace(
				bodies.back().id(),
				std::exchange(index, Random(polygons.size() - 1))
			);
		}

		// ------------------------
		// 弾と的の当たり判定
		// -----------------------
		for (auto t = targets.begin(); t != targets.end();)
		{
			bool hit = false;

			

			for (const auto& body : bodies)
			{
				Circle bullet(body.getPos(), 20);
				if (t->circle.intersects(bullet))
				{
					hit = true;
					break;
				}
			}

			if (hit)
			{
				if (t->isBad)
				{
					if (isFever)
					{
						score += 1;
						CreateExplosion(t->circle.center,Palette::Gold);
						popups << ScorePopup{t->circle.center,U"+1",Palette::Gold};
						hitSE.playOneShot(0.2);
					}
					else
					{
						score -= 1;
						CreateExplosion(t->circle.center,Palette::Skyblue);
						popups << ScorePopup{t->circle.center,U"-1",Palette::Skyblue};
						if (!isFever)
						{
							--feverGauge;
							if (feverGauge < 0)
							{
								feverGauge = 0;
							}
						}
						missSE.playOneShot(0.2);
						shakeTime = 0.3;
					}
				}
				else
				{
					score += 1;
					CreateExplosion(t->circle.center,isFever ? Palette::Gold : Palette::Red);
					popups << ScorePopup{t->circle.center,U"+1",Palette::Yellow};
					if (!isFever)
					{
						++feverGauge;

						if (feverGauge >= FeverNeed)
						{
							isFever = true;
							feverTime = FeverDuration;
							feverGauge = 0;
							feverSE.playOneShot(0.5);
						}
					}
					hitSE.playOneShot(0.2);
				}

				t = targets.erase(t);
			}
			else if (t->timer.sF() >= 0.8) // 0.8秒経過
			{
				t = targets.erase(t);
			}
			else
			{
				++t;
			}
		}

		// 画面外の弾を削除
		for (auto it = bodies.begin(); it != bodies.end();)
		{
			const Vec2 pos = it->getPos();

			if (pos.x < -1000 || pos.x > 1480
				|| pos.y < -1000 || pos.y > 920)
			{
				table.erase(it->id());
				it = bodies.erase(it);
			}
			else
			{
				++it;
			}
		}

		// ------------------------
		// 的描画
		// ------------------------

		{
			const Transformer2D transformer{ Mat3x2::Translate(shakeOffset) };

			for (const auto& target : targets)
			{
				if (isFever)
				{
					target.circle.draw(Palette::Gold);
				}
				else if (target.isBad)
				{
					target.circle.draw(Palette::Blue);
				}
				else
				{
					target.circle.draw(Palette::Red);
				}
			}

			for (const auto& e : effects)
			{
				Circle(e.pos, e.radius)
					.draw(ColorF(e.color.r, e.color.g, e.color.b, e.life));
			}

			for (const auto& p : popups)
			{
				font(p.text)
					.drawAt(p.pos,ColorF(p.color.r,p.color.g,p.color.b,p.life));
			}

			// 弾描画
			for (const auto& body : bodies)
			{
				textures[table[body.id()]]
					.scaled(0.5)
					.rotated(body.getAngle())
					.drawAt(body.getPos());
			}

			// 銃描画
			if (facingRight)
			{
				cursorTexture.scaled(0.3).drawAt(Cursor::PosF());
			}
			else
			{
				cursorTexture.mirrored()
					.scaled(0.3)
					.drawAt(Cursor::PosF());
			}

		}

		font(U"Score: {}"_fmt(score))
			.draw(20, 20, ColorF{ 1.0 });

		font(U"Time: {:.1f}"_fmt(timeLimit))
			.draw(20, 70, ColorF{ 1.0 });

		if (!isFever)
		{
			font(U"FEVER {}/{}"_fmt(
				feverGauge,
				FeverNeed))
				.draw(20, 120, Palette::Yellow);

		}

		if (isFever)
		{
			font(U"FEVER TIME {:.1f}"_fmt(feverTime))
				.draw(20, 120, Palette::Orange);
		}

		if (isFever)
		{
			Scene::Rect()
				.draw(ColorF(1.0, 0.8, 0.0, 0.15));
		}
		// ------------------------
		// リザルト画面
		// ------------------------
		String rank;

		Color rankColor;

		if (score >= 70)
		{
			rank = U"S";
			rankColor = Palette::Gold;
		}
		else if (score >= 55)
		{
			rank = U"A";
			rankColor = Palette::Orange;
		}
		else if (score >= 40)
		{
			rank = U"B";
			rankColor = Palette::Skyblue;
		}
		else
		{
			rank = U"C";
			rankColor = Palette::Gray;
		}

		if (gameState == GameState::Result)
		{
			Scene::Rect()
				.draw(ColorF{ 0,0,0,0.6 });

			const Vec2 center = Scene::Center();

			font(U"RESULT")
				.drawAt(center.x, center.y - 150, Palette::Yellow);

			font(U"SCORE : {}"_fmt(score))
				.drawAt(center.x, center.y - 50, Palette::White);

			font(U"RANK : {}"_fmt(rank))
				.drawAt(center.x,center.y + 10,rankColor);

			font(U"Press R to Retry")
				.drawAt(center.x, center.y + 80, Palette::Orange);

			font(U"Press T to Title")
				.drawAt(center.x, center.y + 150, Palette::Skyblue);
		}

		if (gameState == GameState::Result && KeyR.down())
		{
			score = 0;

			feverGauge = 0;
			isFever = false;
			feverTime = 0.0;
			shakeTime = 0.0;

			timeLimit = 30.0;

			gameState = GameState::Playing;

			targets.clear();

			bodies.clear();

			table.clear();
		}

		if (gameState == GameState::Result && KeyT.down())
		{
			score = 0;
			timeLimit = 30.0;
			feverGauge = 0;
			isFever = false;
			feverTime = 0.0;
			shakeTime = 0.0;

			targets.clear();
			bodies.clear();
			table.clear();

			gameState = GameState::Title;
		}
	}
}
