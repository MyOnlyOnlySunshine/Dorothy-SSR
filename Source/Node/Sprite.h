/* Copyright (c) 2017 Jin Li, http://www.luvfight.me

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#pragma once

NS_DOROTHY_BEGIN

struct SpriteVertex
{
	float x;
	float y;
	float z;
	float w;
	float u;
	float v;
	uint32_t abgr;
	struct Init
	{
		Init()
		{
			ms_decl.begin()
				.add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
		}
	};
	static bgfx::VertexDecl ms_decl;
	static Init init;
};

class Sprite : public Node
{
public:
	PROPERTY(SpriteEffect*, Effect);
	PROPERTY(Texture2D*, Texture);
	PROPERTY_REF(Rect, TextureRect);
	PROPERTY_REF(BlendFunc, BlendFunc);
	PROPERTY_BOOL(DepthWrite);
	PROPERTY_READONLY(Uint64, RenderState);
	PROPERTY_READONLY(const SpriteVertex*, Vertices);
	virtual ~Sprite();
	virtual bool init() override;
	virtual void render() override;
	virtual const float* getWorld() override;
	CREATE_FUNC(Sprite);
protected:
	Sprite();
	Sprite(String filename);
	Sprite(Texture2D* texture);
	Sprite(Texture2D* texture, const Rect& textureRect);
	void updateVertTexCoord();
	void updateVertPosition();
	void updateVertColor();
	virtual void updateRealColor3() override;
	virtual void updateRealOpacity() override;
private:
	Rect _textureRect;
	Ref<SpriteEffect> _effect;
	Ref<Texture2D> _texture;
	Vec4 _positions[4];
	SpriteVertex _vertices[4];
	BlendFunc _blendFunc;
	Uint64 _renderState;
	enum
	{
		VertexColorDirty = Node::UserFlag,
		VertexPosDirty = Node::UserFlag<<1,
		DepthWrite = Node::UserFlag<<2
	};
	DORA_TYPE_OVERRIDE(Sprite);
};

class SpriteRenderer
{
public:
	PROPERTY_READONLY(SpriteEffect*, DefaultEffect);
	virtual ~SpriteRenderer();
	void render(Sprite* sprite = nullptr);
protected:
	SpriteRenderer();
	void doRender();
private:
	Ref<SpriteEffect> _defaultEffect;
	Texture2D* _lastTexture;
	SpriteEffect* _lastEffect;
	Uint64 _lastState;
	vector<SpriteVertex> _vertices;
	const uint16_t _spriteIndices[6];
	SINGLETON(SpriteRenderer, "BGFXDora");
};

#define SharedSpriteRenderer \
	Dorothy::Singleton<Dorothy::SpriteRenderer>::shared()

NS_DOROTHY_END
