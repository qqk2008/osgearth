/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
 * Copyright 2008-2014 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include "DrawTileCommand"

using namespace osgEarth::REX;

#undef  LC
#define LC "[DrawTileCommand] "

void
DrawTileCommand::draw(osg::RenderInfo& ri, DrawState& dsMaster, osg::Referenced* layerData) const
{
    PerContextDrawState& ds = dsMaster.getPCDS(ri.getContextID());

    osg::State& state = *ri.getState();
        
    // Tile key encoding, if the uniform is required.
    if (ds._tileKeyUL >= 0 )
    {
        ds._ext->glUniform4fv(ds._tileKeyUL, 1, _keyValue.ptr());
    }

    // Apply the layer draw order for this tile so we can blend correctly:
    if (ds._layerOrderUL >= 0 && !ds._layerOrder.isSetTo(_layerOrder))
    {
        ds._ext->glUniform1i(ds._layerOrderUL, (GLint)_layerOrder);
        ds._layerOrder = _layerOrder;
    }

    // Elevation coefficients (can probably be terrain-wide)
    if (ds._elevTexelCoeffUL >= 0 && !ds._elevTexelCoeff.isSetTo(_elevTexelCoeff))
    {
        ds._ext->glUniform2fv(ds._elevTexelCoeffUL, 1, _elevTexelCoeff.ptr());
        ds._elevTexelCoeff = _elevTexelCoeff;
    }

    // Morphing constants for this LOD
    if (ds._morphConstantsUL >= 0 && !ds._morphConstants.isSetTo(_morphConstants))
    {
        ds._ext->glUniform2fv(ds._morphConstantsUL, 1, _morphConstants.ptr());
        ds._morphConstants = _morphConstants;
    }

    // MVM for this tile:
    state.applyModelViewMatrix(_modelViewMatrix.get());
    
    // MVM uniforms for GL3 core:
    if (state.getUseModelViewAndProjectionUniforms())
    {
        state.applyModelViewAndProjectionUniformsIfRequired();
    }

    // Apply samplers for this tile draw:
    unsigned s = 0;

    if (_colorSamplers)
    {
        for (s = 0; s <= SamplerBinding::COLOR_PARENT; ++s)
        {
            const Sampler& sampler = (*_colorSamplers)[s];
            SamplerState& samplerState = ds._samplerState._samplers[s];

            if (sampler._texture.valid() && !samplerState._texture.isSetTo(sampler._texture.get()))
            {
                state.setActiveTextureUnit((*dsMaster._bindings)[s].unit());
                sampler._texture->apply(state);
                samplerState._texture = sampler._texture.get();
            }

            if (samplerState._matrixUL >= 0 && !samplerState._matrix.isSetTo(sampler._matrix))
            {
                ds._ext->glUniformMatrix4fv(samplerState._matrixUL, 1, GL_FALSE, sampler._matrix.ptr());
                samplerState._matrix = sampler._matrix;
            }

            // Need a special uniform for color parents.
            if (s == SamplerBinding::COLOR_PARENT)
            {
                if (ds._parentTextureExistsUL >= 0 && !ds._parentTextureExists.isSetTo(sampler._texture.get() != 0L))
                {
                    ds._ext->glUniform1f(ds._parentTextureExistsUL, sampler._texture.valid() ? 1.0f : 0.0f);
                    ds._parentTextureExists = sampler._texture.valid();
                }
            }
        }
    }

    if (_sharedSamplers)
    {
        for (; s < _sharedSamplers->size(); ++s)
        {
            const Sampler& sampler = (*_sharedSamplers)[s];
            SamplerState& samplerState = ds._samplerState._samplers[s];

            if (sampler._texture.valid() && !samplerState._texture.isSetTo(sampler._texture.get()))
            {
                state.setActiveTextureUnit((*dsMaster._bindings)[s].unit());
                sampler._texture->apply(state);
                samplerState._texture = sampler._texture.get();
            }

            if (samplerState._matrixUL >= 0 && !samplerState._matrix.isSetTo(sampler._matrix))
            {
                ds._ext->glUniformMatrix4fv(samplerState._matrixUL, 1, GL_FALSE, sampler._matrix.ptr());
                samplerState._matrix = sampler._matrix;
            }
        }
    }

    if (_drawCallback)
    {
        PatchLayer::DrawContext dc;

        dc._key = _key;
        dc._range = _range;
        dc._geom = _geom.get();
        _drawCallback->draw(ri, dc, layerData);
    }

    else
    // If there's a geometry, draw it now:
    if (_geom.valid())
    {
        _geom->_ptype[ri.getContextID()] = _drawPatch ? GL_PATCHES : _geom->getDrawElements()->getMode();
        _geom->draw(ri);
    }    
}
