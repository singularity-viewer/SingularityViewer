/**
 * @file lldaeloader.cpp
 * @brief LLDAELoader class implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2013, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include <dae.h>
#include <dom/domAsset.h>
#include <dom/domBind_material.h>
#include <dom/domCOLLADA.h>
#include <dom/domConstants.h>
#include <dom/domController.h>
#include <dom/domEffect.h>
#include <dom/domGeometry.h>
#include <dom/domInstance_geometry.h>
#include <dom/domInstance_material.h>
#include <dom/domInstance_node.h>
#include <dom/domInstance_effect.h>
#include <dom/domMaterial.h>
#include <dom/domMatrix.h>
#include <dom/domNode.h>
#include <dom/domProfile_COMMON.h>
#include <dom/domRotate.h>
#include <dom/domScale.h>
#include <dom/domTranslate.h>
#include <dom/domVisual_scene.h>

#include <boost/regex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/iterator_adaptors.hpp>
#include <boost/range/adaptor/indexed.hpp>

#include "lldaeloader.h"
#include "llsdserialize.h"
#include "lljoint.h"

#include "llmatrix4a.h"

std::string colladaVersion[VERSIONTYPE_COUNT + 1] =
{
	"1.4.0",
	"1.4.1",
	"Unsupported"
};

static const std::string lod_suffix[LLModel::NUM_LODS] =
{
	"_LOD0",
	"_LOD1",
	"_LOD2",
	"",
	"_PHYS",
};

const U32 LIMIT_MATERIALS_OUTPUT = 12;
bool get_dom_sources(const domInputLocalOffset_Array& inputs, U32& pos_offset, U32& tc_offset, U32& norm_offset, U32& idx_stride,
	domSource*& pos_source, domSource*& tc_source, domSource*& norm_source)
{
	idx_stride = 0;
	for (size_t j = 0; j < inputs.getCount(); ++j)
	{
		const auto& input = inputs[j];
		const auto input_semantic = input->getSemantic();
		// Offset value sanitization / fault tolerance
		idx_stride = (U32)llmax((S32)input->getOffset(), (S32)idx_stride);
		if (strcmp(COMMON_PROFILE_INPUT_VERTEX, input_semantic) == 0)
		{
			//found vertex array
			const auto& uri = input->getSource();
			const auto elem = uri.getElement();
			const auto vertices = (domVertices*)elem.cast();
			if (!vertices)
			{
				return false;
			}

			const auto& v_inp = vertices->getInput_array();
			for (size_t k = 0; k < v_inp.getCount(); ++k)
			{
				const auto& v_inp_k = v_inp[k];
				const auto v_inp_semantic = v_inp_k->getSemantic();
				if (strcmp(COMMON_PROFILE_INPUT_POSITION, v_inp_semantic) == 0)
				{
					pos_offset = input->getOffset();
					const auto& uri = v_inp_k->getSource();
					const auto elem = uri.getElement();
					pos_source = (domSource*)elem.cast();
				}

				if (strcmp(COMMON_PROFILE_INPUT_NORMAL, v_inp_semantic) == 0)
				{
					norm_offset = input->getOffset();
					const auto& uri = v_inp_k->getSource();
					const auto elem = uri.getElement();
					norm_source = (domSource*)elem.cast();
				}
			}
		}


		if (strcmp(COMMON_PROFILE_INPUT_NORMAL, input_semantic) == 0)
		{
			//found normal array for this triangle list
			norm_offset = input->getOffset();
			const auto& uri = input->getSource();
			const auto elem = uri.getElement();
			norm_source = (domSource*)elem.cast();
		}
		else if (strcmp(COMMON_PROFILE_INPUT_TEXCOORD, input_semantic) == 0)
		{
			//found texCoords
			tc_offset = input->getOffset();
			const auto& uri = input->getSource();
			const auto elem = uri.getElement();
			tc_source = (domSource*)elem.cast();
		}
	}

	idx_stride += 1;

	return true;
}

LLModel::EModelStatus load_face_from_dom_triangles(std::vector<LLVolumeFace>& face_list, std::vector<std::string>& materials, domTrianglesRef& tri)
{
	LLVolumeFace face;
	std::vector<LLVolumeFace::VertexData> verts;
	std::vector<U16> indices;

	const auto& inputs = tri->getInput_array();

	U32 pos_offset, tc_offset, norm_offset, idx_stride;
	domSource* pos_source = NULL, * tc_source = NULL, * norm_source = NULL;

	if (!get_dom_sources(inputs, pos_offset, tc_offset, norm_offset, idx_stride, pos_source, tc_source, norm_source) || !pos_source)
	{
		LL_WARNS() << "Could not find dom sources for basic geo data; invalid model." << LL_ENDL;
		return LLModel::BAD_ELEMENT;
	}

	if (!pos_source || !pos_source->getFloat_array())
	{
		LL_WARNS() << "Unable to process mesh without position data; invalid model;  invalid model." << LL_ENDL;
		return LLModel::BAD_ELEMENT;
	}

	const auto p = tri->getP();
	const auto& idx = p->getValue();

	domListOfFloats  dummy;
	const auto& v = pos_source ? pos_source->getFloat_array()->getValue() : dummy;
	const auto& tc = tc_source ? tc_source->getFloat_array()->getValue() : dummy;
	const auto& n = norm_source ? norm_source->getFloat_array()->getValue() : dummy;

	const auto index_count = idx.getCount();
	const auto vertex_count = pos_source ? v.getCount() : 0;
	const auto tc_count = tc_source ? tc.getCount() : 0;
	const auto norm_count = norm_source ? n.getCount() : 0;

	if (pos_source)
	{
		if (vertex_count == 0)
		{
			LL_WARNS() << "Unable to process mesh with empty position array; invalid model." << LL_ENDL;
			return LLModel::BAD_ELEMENT;
		}
	}

	face.mExtents[0].set(v[0], v[1], v[2]);
	face.mExtents[1].set(v[0], v[1], v[2]);

	LLVolumeFace::VertexMapData::PointMap point_map;

	for (size_t i = 0; i < index_count; i += idx_stride)
	{
		LLVolumeFace::VertexData cv;
		if (pos_source)
		{
			// guard against model data specifiying out of range indices or verts
			//
			const auto p_pos_index = idx[i + pos_offset] * 3;
			if (((i + pos_offset) > index_count)
				|| ((p_pos_index + 2) > vertex_count))
			{
				LL_WARNS() << "Out of range index data; invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}

			const auto cv_position = LLVector4a(
				v[p_pos_index],
				v[p_pos_index + 1],
				v[p_pos_index + 2]);
			cv.setPosition(cv_position);

			if (!cv_position.isFinite3())
			{
				LL_WARNS() << "Nan positional data, invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}
		}

		if (tc_source)
		{
			// guard against model data specifiying out of range indices or tcs
			//
			const auto p_tc_index = idx[i + tc_offset] * 2;
			if (((i + tc_offset) > index_count)
				|| ((p_tc_index + 1) > tc_count))
			{
				LL_WARNS() << "Out of range TC indices." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}

			cv.mTexCoord = LLVector2(
				tc[p_tc_index],
				tc[p_tc_index + 1]);

			if (!cv.mTexCoord.isFinite())
			{
				LL_WARNS() << "Found NaN while loading tex coords from DAE-Model, invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}
		}

		if (norm_source)
		{
			// guard against model data specifiying out of range indices or norms
			//
			const auto p_norm_index = idx[i + norm_offset] * 3;
			if (((i + norm_offset) > index_count)
				|| ((p_norm_index + 2) > norm_count))
			{
				LL_WARNS() << "Found out of range norm indices, invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}

			const auto cv_normal = LLVector4a(
				n[p_norm_index],
				n[p_norm_index + 1],
				n[p_norm_index + 2]);
			cv.setNormal(cv_normal);

			if (!cv_normal.isFinite3())
			{
				LL_WARNS() << "Found NaN while loading normals from DAE-Model, invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}
		}

		BOOL found = FALSE;

		const auto pos3 = LLVector3(cv.getPosition().getF32ptr());
		const auto point_iter = point_map.find(pos3);

		if (point_iter != point_map.end())
		{
			const auto& vm_data = point_iter->second;
			for (const auto& vm : vm_data)
			{
				// We have a matching loc
				//
				if (vm == cv)
				{
					// Don't share verts within the same tri, degenerate
					//
					const auto index_size = indices.size();
					const auto verts_new_tri = index_size % 3;
					if ((verts_new_tri < 1 || indices[index_size - 1] != vm.mIndex)
						&& (verts_new_tri < 2 || indices[index_size - 2] != vm.mIndex))
					{
						found = true;
						indices.push_back(vm.mIndex);
					}
					break;
				}
			}
		}

		if (!found)
		{
			update_min_max(face.mExtents[0], face.mExtents[1], cv.getPosition());
			verts.push_back(cv);
			if (verts.size() >= 0xFFFFU)
			{
				//LL_ERRS() << "Attempted to write model exceeding 16-bit index buffer limitation." << LL_ENDL;
				return LLModel::VERTEX_NUMBER_OVERFLOW;
			}
			const auto index = (U16)(verts.size() - 1);
			indices.push_back(index);

			LLVolumeFace::VertexMapData d;
			d.setPosition(cv.getPosition());
			d.mTexCoord = cv.mTexCoord;
			d.setNormal(cv.getNormal());
			d.mIndex = index;

			if (point_iter != point_map.end())
			{
				point_iter->second.push_back(d);
			}
			else
			{
				const auto point = LLVector3(d.getPosition().getF32ptr());
				point_map[point].push_back(d);
			}
		}

		if (indices.size() % 3 == 0 && verts.size() >= 0xFFFCU)
		{
			std::string material;

			if (tri->getMaterial())
			{
				material = std::string(tri->getMaterial());
			}

			materials.push_back(material);
			face_list.push_back(face);
			face_list.rbegin()->fillFromLegacyData(verts, indices);
			auto& new_face = *face_list.rbegin();
			if (!norm_source)
			{
				//ll_aligned_free_16(new_face.mNormals);
				new_face.mNormals = NULL;
			}

			if (!tc_source)
			{
				//ll_aligned_free_16(new_face.mTexCoords);
				new_face.mTexCoords = NULL;
			}

			face = LLVolumeFace();
			point_map.clear();
		}
	}

	if (!verts.empty())
	{
		std::string material;

		if (tri->getMaterial())
		{
			material = std::string(tri->getMaterial());
		}

		materials.push_back(material);
		face_list.push_back(face);

		face_list.rbegin()->fillFromLegacyData(verts, indices);
		auto& new_face = *face_list.rbegin();
		if (!norm_source)
		{
			//ll_aligned_free_16(new_face.mNormals);
			new_face.mNormals = NULL;
		}

		if (!tc_source)
		{
			//ll_aligned_free_16(new_face.mTexCoords);
			new_face.mTexCoords = NULL;
		}
	}

	return LLModel::NO_ERRORS;
}

LLModel::EModelStatus load_face_from_dom_polylist(std::vector<LLVolumeFace>& face_list, std::vector<std::string>& materials, domPolylistRef& poly)
{
	const auto p = poly->getP();
	const auto& idx = p->getValue();

	const auto index_count = idx.getCount();
	if (index_count == 0)
	{
		return LLModel::NO_ERRORS;
	}

	const auto& inputs = poly->getInput_array();
	const auto& vcount = poly->getVcount()->getValue();

	auto pos_offset = 0U, tc_offset = 0U, norm_offset = 0U, idx_stride = 0U;
	domSource* pos_source = NULL, * tc_source = NULL, * norm_source = NULL;

	if (!get_dom_sources(inputs, pos_offset, tc_offset, norm_offset, idx_stride, pos_source, tc_source, norm_source))
	{
		LL_WARNS() << "Could not get DOM sources for basic geo data, invalid model." << LL_ENDL;
		return LLModel::BAD_ELEMENT;
	}

	LLVolumeFace face;

	std::vector<U16> indices;
	std::vector<LLVolumeFace::VertexData> verts;

	domListOfFloats v, tc, n;

	if (pos_source)
	{
		v = pos_source->getFloat_array()->getValue();
		face.mExtents[0].set(v[0], v[1], v[2]);
		face.mExtents[1].set(v[0], v[1], v[2]);
	}

	if (tc_source)
	{
		tc = tc_source->getFloat_array()->getValue();
	}

	if (norm_source)
	{
		n = norm_source->getFloat_array()->getValue();
	}

	LLVolumeFace::VertexMapData::PointMap point_map;

	const auto vertex_count = pos_source ? v.getCount() : 0;
	const auto tc_count = tc_source ? tc.getCount() : 0;
	const auto norm_count = norm_source ? n.getCount() : 0;

	size_t cur_idx = 0;
	for (size_t i = 0; i < vcount.getCount(); ++i)
	{
		//for each polygon
		auto first_index = (U16)0;
		auto last_index = (U16)0;
		for (size_t j = 0; j < vcount[i]; ++j)
		{
			//for each vertex
			LLVolumeFace::VertexData cv;

			if (pos_source)
			{
				// guard against model data specifiying out of range indices or verts
				//
				const auto cur_idx_offset = cur_idx + pos_offset;
				const auto p_pos_index = (size_t)idx[cur_idx_offset] * 3;
				if ((cur_idx_offset > index_count) || ((p_pos_index + 2) > vertex_count))
				{
					LL_WARNS() << "Out of range position indices, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}

				const auto cv_position = LLVector4a(
					v[p_pos_index],
					v[p_pos_index + 1],
					v[p_pos_index + 2]);
				cv.setPosition(cv_position);

				if (!cv_position.isFinite3())
				{
					LL_WARNS() << "Found NaN while loading position data from DAE-Model, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}

			}

			if (tc_source)
			{
				// guard against model data specifiying out of range indices or tcs
				//
				const auto cur_idx_offset = cur_idx + tc_offset;
				const auto p_tc_index = (size_t)idx[cur_idx_offset] * 2;
				if ((cur_idx_offset > index_count) || ((p_tc_index + 1) > tc_count))
				{
					LL_WARNS() << "Out of range TC indices, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}

				cv.mTexCoord = LLVector2(
					tc[p_tc_index],
					tc[p_tc_index + 1]);

				if (!cv.mTexCoord.isFinite())
				{
					LL_WARNS() << "Found NaN while loading tex coords from DAE-Model, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}
			}

			if (norm_source)
			{
				// guard against model data specifiying out of range indices or norms
				//
				const auto cur_idx_offset = cur_idx + norm_offset;
				const auto p_norm_index = (size_t)idx[cur_idx_offset] * 3;
				if ((cur_idx_offset > index_count) || ((p_norm_index + 2) > norm_count))
				{
					LL_WARNS() << "Out of range norm indices, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}

				const auto cv_normal = LLVector4a(
					n[p_norm_index],
					n[p_norm_index + 1],
					n[p_norm_index + 2]);
				cv.setNormal(cv_normal);

				if (!cv_normal.isFinite3())
				{
					LL_WARNS() << "Found NaN while loading normals from DAE-Model, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}
			}

			cur_idx += idx_stride;

			BOOL found = FALSE;

			LLVector3 pos3(cv.getPosition().getF32ptr());
			const auto point_iter = point_map.find(pos3);
			if (point_iter != point_map.end())
			{
				const auto& vm_data = point_iter->second;
				for (const auto& vm : vm_data)
				{
					// If vertex data matches current vertex
					if (vm == cv)
					{
						found = TRUE;
						const auto index = vm.mIndex;
						if (j == 0)
						{
							first_index = index;
						}
						else if (j == 1)
						{
							last_index = index;
						}
						else
						{
							// if these are the same, we have a very, very skinny triangle (coincident verts on one or more edges)
							//
							llassert((first_index != last_index) && (last_index != index) && (first_index != index));
							indices.push_back(first_index);
							indices.push_back(last_index);
							indices.push_back(index);
							last_index = index;
						}

						break;
					}
				}
			}

			if (!found)
			{
				update_min_max(face.mExtents[0], face.mExtents[1], cv.getPosition());
				verts.push_back(cv);
				if (verts.size() >= 0xFFFFU)
				{
					//LL_ERRS() << "Attempted to write model exceeding 16-bit index buffer limitation." << LL_ENDL;
					return LLModel::VERTEX_NUMBER_OVERFLOW;
				}
				const auto index = (U16)(verts.size() - 1);

				if (j == 0)
				{
					first_index = index;
				}
				else if (j == 1)
				{
					last_index = index;
				}
				else
				{
					// detect very skinny degenerate triangles with collapsed edges
					//
					llassert((first_index != last_index) && (last_index != index) && (first_index != index));
					indices.push_back(first_index);
					indices.push_back(last_index);
					indices.push_back(index);
					last_index = index;
				}

				LLVolumeFace::VertexMapData d;
				d.setPosition(cv.getPosition());
				d.mTexCoord = cv.mTexCoord;
				d.setNormal(cv.getNormal());
				d.mIndex = index;

				if (point_iter != point_map.end())
				{
					point_iter->second.push_back(d);
				}
				else
				{
					point_map[pos3].push_back(d);
				}
			}

			if (indices.size() % 3 == 0 && indices.size() >= 0xFFFCU)
			{
				std::string material;

				if (poly->getMaterial())
				{
					material = std::string(poly->getMaterial());
				}

				materials.push_back(material);
				face_list.push_back(face);
				face_list.rbegin()->fillFromLegacyData(verts, indices);

				auto& new_face = *face_list.rbegin();
				if (!norm_source)
				{
					//ll_aligned_free_16(new_face.mNormals);
					new_face.mNormals = NULL;
				}

				if (!tc_source)
				{
					//ll_aligned_free_16(new_face.mTexCoords);
					new_face.mTexCoords = NULL;
				}

				face = LLVolumeFace();
				verts.clear();
				indices.clear();
				point_map.clear();
			}
		}
	}

	if (!verts.empty())
	{
		std::string material;

		if (poly->getMaterial())
		{
			material = std::string(poly->getMaterial());
		}

		materials.push_back(material);
		face_list.push_back(face);
		face_list.rbegin()->fillFromLegacyData(verts, indices);

		auto& new_face = *face_list.rbegin();
		if (!norm_source)
		{
			//ll_aligned_free_16(new_face.mNormals);
			new_face.mNormals = NULL;
		}

		if (!tc_source)
		{
			//ll_aligned_free_16(new_face.mTexCoords);
			new_face.mTexCoords = NULL;
		}
	}

	return LLModel::NO_ERRORS;
}

LLModel::EModelStatus load_face_from_dom_polygons(std::vector<LLVolumeFace>& face_list, std::vector<std::string>& materials, domPolygonsRef& poly)
{
	LLVolumeFace face;
	std::vector<U16> indices;
	std::vector<LLVolumeFace::VertexData> verts;

	const auto& inputs = poly->getInput_array();

	auto v_offset = 0U, n_offset = 0U, t_offset = 0U, stride = 0U;
	domListOfFloats* v = NULL, * n = NULL, * t = NULL;

	for (size_t i = 0; i < inputs.getCount(); ++i)
	{
		const auto& input = inputs[i];
		const auto input_semantic = input->getSemantic();
		// Offset value sanitization / fault tolerance
		stride = (U32)llmax((S32)input->getOffset() + 1, (S32)stride);
		if (strcmp(COMMON_PROFILE_INPUT_VERTEX, input_semantic) == 0)
		{
			//found vertex array
			v_offset = input->getOffset();

			const auto& uri = input->getSource();
			const auto elem = uri.getElement();
			const auto vertices = (domVertices*)elem.cast();
			if (!vertices)
			{
				LL_WARNS() << "Could not find vertex source, invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}
			const auto& v_inp = vertices->getInput_array();
			for (size_t k = 0; k < v_inp.getCount(); ++k)
			{
				auto& v_inp_k = v_inp[k];
				if (strcmp(COMMON_PROFILE_INPUT_POSITION, v_inp_k->getSemantic()) == 0)
				{
					const auto& uri = v_inp_k->getSource();
					const auto elem = uri.getElement();
					const auto src = (domSource*)elem.cast();
					if (!src)
					{
						LL_WARNS() << "Could not find DOM source, invalid model." << LL_ENDL;
						return LLModel::BAD_ELEMENT;
					}
					v = &(src->getFloat_array()->getValue());
				}
			}
		}
		else if (strcmp(COMMON_PROFILE_INPUT_NORMAL, input_semantic) == 0)
		{
			//found normal array for this triangle list
			n_offset = input->getOffset();
			const auto& uri = input->getSource();
			const auto elem = uri.getElement();
			const auto src = (domSource*)elem.cast();
			if (!src)
			{
				LL_WARNS() << "Could not find DOM source, invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}
			n = &(src->getFloat_array()->getValue());
		}
		else if (strcmp(COMMON_PROFILE_INPUT_TEXCOORD, input_semantic) == 0 && input->getSet() == 0)
		{
			//found texCoords
			t_offset = input->getOffset();
			const auto& uri = input->getSource();
			const auto elem = uri.getElement();
			const auto src = (domSource*)elem.cast();
			if (!src)
			{
				LL_WARNS() << "Could not find DOM source, invalid model." << LL_ENDL;
				return LLModel::BAD_ELEMENT;
			}
			t = &(src->getFloat_array()->getValue());
		}
	}

	const auto& ps = poly->getP_array();

	//make a triangle list in <verts>
	for (size_t i = 0; i < ps.getCount(); ++i)
	{
		//for each polygon
		const auto& idx = ps[i]->getValue();
		const auto idx_count_by_stride = idx.getCount() / stride;
		for (size_t j = 0; j < idx_count_by_stride; ++j)
		{
			//for each vertex
			if (j > 2)
			{
				const auto& v0 = verts[(U32)verts.size() - 3];
				const auto& v1 = verts[(U32)verts.size() - 1];

				verts.push_back(v0);
				verts.push_back(v1);
			}

			LLVolumeFace::VertexData vert;

			if (v)
			{
				auto v_idx = (size_t)idx[j * stride + v_offset] * 3;
				v_idx = llclamp(v_idx, size_t(0), v->getCount());
				const auto v_pos = LLVector4a(
					v->get(v_idx),
					v->get(v_idx + 1),
					v->get(v_idx + 2));
				vert.setPosition(v_pos);
				if (!v_pos.isFinite3())
				{
					LL_WARNS() << "Found NaN while loading position data from DAE-Model, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}
			}

			//bounds check n and t lookups because some FBX to DAE converters
			//use negative indices and empty arrays to indicate data does not exist
			//for a particular channel
			if (n && n->getCount() > 0)
			{
				auto n_idx = (size_t)idx[j * stride + n_offset] * 3;
				n_idx = llclamp(n_idx, size_t(0), n->getCount());
				const auto v_norm = LLVector4a(
					n->get(n_idx),
					n->get(n_idx + 1),
					n->get(n_idx + 2));
				vert.setNormal(v_norm);
				if (!v_norm.isFinite3())
				{
					LL_WARNS() << "Found NaN while loading normals from DAE-Model, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}
			}
			else
			{
				vert.getNormal().clear();
			}

			if (t && t->getCount() > 0)
			{
				auto t_idx = (size_t)idx[j * stride + t_offset] * 2;
				t_idx = llclamp(t_idx, size_t(0), t->getCount());
				vert.mTexCoord = LLVector2(
					t->get(t_idx),
					t->get(t_idx + 1));
				if (!vert.mTexCoord.isFinite())
				{
					LL_WARNS() << "Found NaN while loading tex coords from DAE-Model, invalid model." << LL_ENDL;
					return LLModel::BAD_ELEMENT;
				}
			}
			else
			{
				vert.mTexCoord.clear();
			}

			verts.push_back(vert);
		}
	}

	if (verts.empty())
	{
		return LLModel::NO_ERRORS;
	}

	face.mExtents[0] = verts[0].getPosition();
	face.mExtents[1] = verts[0].getPosition();

	//create a map of unique vertices to indices
	std::map<LLVolumeFace::VertexData, U32> vert_idx;

	auto cur_idx = 0U;
	for (size_t i = 0; i < verts.size(); ++i)
	{
		auto iter = vert_idx.find(verts[i]);
		if (iter == vert_idx.end())
		{
			vert_idx[verts[i]] = cur_idx++;
		}
	}

	//build vertex array from map
	std::vector<LLVolumeFace::VertexData> new_verts;
	new_verts.resize(vert_idx.size());

	for (const auto& iter : vert_idx)
	{
		new_verts[iter.second] = iter.first;
		update_min_max(face.mExtents[0], face.mExtents[1], iter.first.getPosition());
	}

	//build index array from map
	indices.resize(verts.size());

	for (size_t i = 0; i < verts.size(); ++i)
	{
		indices[i] = vert_idx[verts[i]];
		llassert(!i || (indices[i - 1] != indices[i]));
	}

	// DEBUG just build an expanded triangle list
	/*for (U32 i = 0; i < verts.size(); ++i)
	{
		indices.push_back((U16) i);
		update_min_max(face.mExtents[0], face.mExtents[1], verts[i].getPosition());
	}*/

	if (!new_verts.empty())
	{
		std::string material;

		if (poly->getMaterial())
		{
			material = std::string(poly->getMaterial());
		}

		materials.push_back(material);
		face_list.push_back(face);
		face_list.rbegin()->fillFromLegacyData(new_verts, indices);

		auto& new_face = *face_list.rbegin();
		if (!n)
		{
			//ll_aligned_free_16(new_face.mNormals);
			new_face.mNormals = NULL;
		}

		if (!t)
		{
			//ll_aligned_free_16(new_face.mTexCoords);
			new_face.mTexCoords = NULL;
		}
	}

	return LLModel::NO_ERRORS;
}

//-----------------------------------------------------------------------------
// LLDAELoader
//-----------------------------------------------------------------------------
LLDAELoader::LLDAELoader(
	std::string				filename,
	S32						lod,
	load_callback_t		load_cb,
	joint_lookup_func_t	joint_lookup_func,
	texture_load_func_t	texture_load_func,
	state_callback_t		state_cb,
	void* opaque_userdata,
	JointTransformMap& jointTransformMap,
	JointNameSet& jointsFromNodes,
	std::map<std::string, std::string>& jointAliasMap,
	U32					maxJointsPerMesh,
	U32					modelLimit,
	bool					preprocess)
	: LLModelLoader(
		filename,
		lod,
		load_cb,
		joint_lookup_func,
		texture_load_func,
		state_cb,
		opaque_userdata,
		jointTransformMap,
		jointsFromNodes,
		jointAliasMap,
		maxJointsPerMesh),
	mGeneratedModelLimit(modelLimit),
	mPreprocessDAE(preprocess)
{
}

LLDAELoader::~LLDAELoader()
{
}

struct ModelSort
{
	bool operator()(const LLPointer< LLModel >& lhs, const LLPointer< LLModel >& rhs)
	{
		if (lhs->mSubmodelID < rhs->mSubmodelID)
		{
			return true;
		}
		return LLStringUtil::compareInsensitive(lhs->mLabel, rhs->mLabel) < 0;
	}
};

bool LLDAELoader::OpenFile(const std::string& filename)
{
	//no suitable slm exists, load from the .dae file
	DAE dae;
	domCOLLADA* dom;
	if (mPreprocessDAE)
	{
		dom = dae.openFromMemory(filename, preprocessDAE(filename).c_str());
	}
	else
	{
		LL_INFOS() << "Skipping dae preprocessing" << LL_ENDL;
		dom = dae.open(filename);
	}

	if (!dom)
	{
		LL_INFOS() << " Error with dae - traditionally indicates a corrupt file." << LL_ENDL;
		setLoadState(ERROR_PARSING);
		return false;
	}

	//Dom version
	const auto domVersion = dae.getDomVersion();
	std::string sldom(domVersion);
	LL_INFOS() << "Collada Importer Version: " << sldom << LL_ENDL;

	//Dae version
	auto docVersion = dom->getVersion();
	//0=1.4
	//1=1.4.1
	//2=Currently unsupported, however may work
	if (docVersion > 1)
	{
		docVersion = VERSIONTYPE_COUNT;
	}
	LL_INFOS() << "Dae version " << colladaVersion[docVersion] << LL_ENDL;

	const auto db = dae.getDatabase();

	const auto doc = dae.getDoc(filename);
	if (!doc)
	{
		LL_WARNS() << "can't find internal doc" << LL_ENDL;
		return false;
	}

	const auto root = doc->getDomRoot();
	if (!root)
	{
		LL_WARNS() << "document has no root" << LL_ENDL;
		return false;
	}

	//Verify some basic properties of the dae
	//1. Basic validity check on controller 
	const auto controllerCount = db->getElementCount(NULL, "controller");
	bool result = false;
	for (size_t i = 0; i < controllerCount; ++i)
	{
		domController* pController = NULL;
		db->getElement((daeElement**)&pController, i, NULL, "controller");
		result = verifyController(pController);
		if (!result)
		{
			LL_INFOS() << "Could not verify controller" << LL_ENDL;
			setLoadState(ERROR_PARSING);
			return true;
		}
	}

	//get unit scale
	mTransform.setIdentity();

	auto unit = daeSafeCast<domAsset::domUnit>(root->getDescendant(daeElement::matchType(domAsset::domUnit::ID())));

	if (unit)
	{
		auto meter = (F32)unit->getMeter();
		mTransform.mMatrix[0][0] = meter;
		mTransform.mMatrix[1][1] = meter;
		mTransform.mMatrix[2][2] = meter;
	}

	//get up axis rotation
	LLMatrix4 rotation;

	auto up = UPAXISTYPE_Y_UP;  // default is Y_UP
	const auto up_axis = daeSafeCast<domAsset::domUp_axis>(root->getDescendant(daeElement::matchType(domAsset::domUp_axis::ID())));

	if (up_axis)
	{
		up = up_axis->getValue();
	}

	if (up == UPAXISTYPE_X_UP)
	{
		rotation.initRotation(0.0f, 90.0f * DEG_TO_RAD, 0.0f);
	}
	else if (up == UPAXISTYPE_Y_UP)
	{
		rotation.initRotation(90.0f * DEG_TO_RAD, 0.0f, 0.0f);
	}

	rotation *= mTransform;
	mTransform = rotation;

	mTransform.condition();

	const auto mesh_count = db->getElementCount(NULL, COLLADA_TYPE_MESH);
	const auto submodel_limit = mesh_count > 0 ? mGeneratedModelLimit / mesh_count : 0;
	for (size_t idx = 0; idx < mesh_count; ++idx)
	{
		//build map of domEntities to LLModel
		domMesh* mesh = NULL;
		db->getElement((daeElement**)&mesh, idx, NULL, COLLADA_TYPE_MESH);

		if (mesh)
		{
			std::vector<LLModel*> models;
			loadModelsFromDomMesh(mesh, models, submodel_limit);
			for (const auto& mdl : models)
			{
				if (mdl->getStatus() != LLModel::NO_ERRORS)
				{
					setLoadState(ERROR_MODEL + mdl->getStatus());
					return false; //abort
				}

				if (mdl && validate_model(mdl))
				{
					mModelList.push_back(mdl);
					mModelsMap[mesh].push_back(mdl);
				}
			}
		}
	}

	std::sort(mModelList.begin(), mModelList.end(), ModelSort());

	for (const auto mdl : mModelList)
	{
		const auto material_count = mdl->mMaterialList.size();
		LL_INFOS() << "Importing " << mdl->mLabel << " model with " << material_count << " material references" << LL_ENDL;

		auto mat_iter = mdl->mMaterialList.begin();
		const auto end_iter = material_count > LIMIT_MATERIALS_OUTPUT
			? mat_iter + LIMIT_MATERIALS_OUTPUT
			: mdl->mMaterialList.end();
		for (; mat_iter != end_iter; ++mat_iter)
		{
			LL_INFOS() << mdl->mLabel << " references " << (*mat_iter) << LL_ENDL;
		}
	}

	const auto skin_count = db->getElementCount(NULL, COLLADA_TYPE_SKIN);
	LL_INFOS() << "Collada skins to be processed: " << skin_count << LL_ENDL;

	const auto scene = root->getDescendant("visual_scene");

	if (!scene)
	{
		LL_WARNS() << "document has no visual_scene" << LL_ENDL;
		setLoadState(ERROR_PARSING);
		return true;
	}

	setLoadState(DONE);

	bool badElement = false;

	processElement(scene, badElement, &dae, root);

	if (badElement)
	{
		LL_INFOS() << "Scene could not be parsed" << LL_ENDL;
		setLoadState(ERROR_PARSING);
	}

	return true;
}

std::string LLDAELoader::preprocessDAE(const std::string filename)
{
	// Open a DAE file for some preprocessing (like removing space characters in IDs), see MAINT-5678
	std::ifstream inFile;
	inFile.open(filename.c_str(), std::ios_base::in);
	std::stringstream strStream;
	strStream << inFile.rdbuf();
	std::string buffer = strStream.str();

	LL_INFOS() << "Preprocessing dae file to remove spaces from the names, ids, etc." << LL_ENDL;

	try
	{
		boost::regex re("\"[\\w\\.@#$-]*(\\s[\\w\\.@#$-]*)+\"");
		boost::sregex_iterator next(buffer.begin(), buffer.end(), re);
		boost::sregex_iterator end;
		while (next != end)
		{
			const auto match = *next;
			auto s = match.str();
			LL_INFOS() << s << " found" << LL_ENDL;
			boost::replace_all(s, " ", "_");
			LL_INFOS() << "Replacing with " << s << LL_ENDL;
			boost::replace_all(buffer, match.str(), s);
			++next;
		}
	}
	catch (boost::regex_error&)
	{
		LL_INFOS() << "Regex error" << LL_ENDL;
	}

	return buffer;
}

void LLDAELoader::processDomModel(LLModel* model, DAE* dae, daeElement* root, domMesh* mesh, domSkin* skin)
{
	llassert(model && dae && mesh && skin);

	if (model)
	{
		LLVector3 mesh_scale_vector;
		LLVector3 mesh_translation_vector;
		model->getNormalizedScaleTranslation(mesh_scale_vector, mesh_translation_vector);

		LLMatrix4 normalized_transformation;
		normalized_transformation.setTranslation(mesh_translation_vector);

		LLMatrix4 mesh_scale;
		mesh_scale.initScale(mesh_scale_vector);
		mesh_scale *= normalized_transformation;
		normalized_transformation = mesh_scale;

		LLMatrix4a inv_mat;
		inv_mat.loadu(normalized_transformation);
		inv_mat.invert();

		LLMatrix4 inverse_normalized_transformation(inv_mat.getF32ptr());

		const auto bind_mat = skin->getBind_shape_matrix();

		if (bind_mat)
		{
			//get bind shape matrix
			const auto& dom_value = bind_mat->getValue();

			auto& skin_info = model->mSkinInfo;

			for (size_t i = 0; i < 4; ++i)
			{
				for (size_t j = 0; j < 4; ++j)
				{
					skin_info.mBindShapeMatrix.mMatrix[i][j] = dom_value[i + j * 4];
				}
			}

			auto trans = normalized_transformation;
			trans *= skin_info.mBindShapeMatrix;
			trans *= mBindTransform;

			skin_info.mBindShapeMatrix = trans;
		}

		// Build the joint to node mapping array and update joint aliases (mJointMap)
		buildJointToNodeMappingFromScene(root);

		//Some collada setup for accessing the skeleton
		const auto skeleton_count = dae->getDatabase()->getElementCount(NULL, "skeleton");
		std::vector<domInstance_controller::domSkeleton*> skeletons;
		for (size_t i = 0; i < skeleton_count; ++i)
		{
			daeElement* pElement = NULL;
			dae->getDatabase()->getElement(&pElement, i, 0, "skeleton");

			//Try to get at the skeletal instance controller
			const auto pSkeleton = daeSafeCast<domInstance_controller::domSkeleton>(pElement);
			daeElement* pSkeletonRootNode = NULL;
			if (pSkeleton)
			{
				pSkeletonRootNode = pSkeleton->getValue().getElement();
			}
			if (pSkeleton && pSkeletonRootNode)
			{
				skeletons.push_back(pSkeleton);
			}
		}
		bool missingSkeletonOrScene = false;

		//If no skeleton, do a breadth-first search to get at specific joints
		if (skeletons.size() == 0)
		{
			const auto pScene = root->getDescendant("visual_scene");
			if (!pScene)
			{
				LL_WARNS() << "No visual scene - unable to parse bone offsets " << LL_ENDL;
				missingSkeletonOrScene = true;
			}
			else
			{
				//Get the children at this level
				const auto children = pScene->getChildren();

				//Process any children that are joints
				//Not all children are joints, some code be ambient lights, cameras, geometry etc..
				for (size_t i = 0; i < children.getCount(); ++i)
				{
					const auto pNode = daeSafeCast<domNode>(children[i]);
					if (isNodeAJoint(pNode))
					{
						processJointNode(pNode, mJointList);
					}
				}
			}
		}
		else
		{
			//Has one or more skeletons
			for (const auto& pSkeleton : skeletons)
			{
				//Get the root node of the skeleton
				if (const auto pSkeletonRootNode = pSkeleton->getValue().getElement())
				{
					//Once we have the root node - start acccessing it's joint components
					//Loop over all the possible joints within the .dae - using the allowed joint list in the ctor.
					for (const auto& jointPair : mJointMap)
					{
						//Build a joint for the resolver to work with and set up the resolver
						char str[64] = { 0 };
						snprintf(str, 64, "./%s", jointPair.first.c_str());
						daeSIDResolver resolver(pSkeletonRootNode, str);

						//Look for the joint
						if (const auto pJoint = daeSafeCast<domNode>(resolver.getElement()))
						{
							// FIXME this has a lot of overlap with processJointNode(), would be nice to refactor.

							//Pull out the translate id and store it in the jointTranslations map
							daeSIDResolver jointResolverA(pJoint, "./translate");
							daeSIDResolver jointResolverB(pJoint, "./location");
							LLMatrix4 workingTransform;


							if (const auto pTranslateA = daeSafeCast<domTranslate>(jointResolverA.getElement()))
							{
								extractTranslation(pTranslateA, workingTransform);
							}
							else if (const auto pTranslateB = daeSafeCast<domTranslate>(jointResolverB.getElement()))
							{
								extractTranslation(pTranslateB, workingTransform);
							}
							else if (const auto pTranslateElement = getChildFromElement(pJoint, "translate"))
							{
								if (const auto pTranslateC = daeSafeCast<domTranslate>(pTranslateElement))
								{
									//Translation via child from element
									extractTranslation(pTranslateC, workingTransform);
								}
								else
								{
									LL_WARNS() << "The found element is not a translate node" << LL_ENDL;
									missingSkeletonOrScene = true;
								}
							}
							else
							{
								//Translation via SID
								extractTranslationViaSID(pJoint, workingTransform);
							}

							//Store the joint transform w/respect to it's name.
							mJointList[jointPair.second.c_str()] = workingTransform;
						}
					}

					//If anything failed in regards to extracting the skeleton, joints or translation id,
					//mention it
					if (missingSkeletonOrScene)
					{
						LL_WARNS() << "Partial jointmap found in asset - did you mean to just have a partial map?" << LL_ENDL;
					}
				} //got skeleton?
			}
		}

		const auto joints = skin->getJoints();

		const auto& joint_input = joints->getInput_array();
		for (size_t i = 0; i < joint_input.getCount(); ++i)
		{
			const auto input = joint_input.get(i);
			const auto semantic = input->getSemantic();
			if (strcmp(semantic, COMMON_PROFILE_INPUT_JOINT) == 0)
			{
				//found joint source, fill model->mJointMap and model->mSkinInfo.mJointNames
				const auto elem = input->getSource().getElement();
				if (const auto source = daeSafeCast<domSource>(elem))
				{
					// TODO: DRY this code
					if (auto names_source = source->getName_array())
					{
						const auto& names = names_source->getValue();
						for (size_t j = 0; j < names.getCount(); ++j)
						{
							std::string name(names.get(j));
							const auto& joint_found = mJointMap.find(name);
							if (joint_found != mJointMap.end())
							{
								name = joint_found->second;
							}
							model->mSkinInfo.mJointNames.push_back(name);
							model->mSkinInfo.mJointNums.push_back(-1);
						}
					}
					else if (auto names_source = source->getIDREF_array())
					{
						const auto& names = names_source->getValue();
						for (size_t j = 0; j < names.getCount(); ++j)
						{
							std::string name(names.get(j).getID());
							const auto& joint_found = mJointMap.find(name);
							if (joint_found != mJointMap.end())
							{
								name = joint_found->second;
							}
							model->mSkinInfo.mJointNames.push_back(name);
							model->mSkinInfo.mJointNums.push_back(-1);
						}
					}
				}
			}
			else if (strcmp(semantic, COMMON_PROFILE_INPUT_INV_BIND_MATRIX) == 0)
			{
				//found inv_bind_matrix array, fill model->mInvBindMatrix
				if (const auto source = daeSafeCast<domSource>(input->getSource().getElement()))
				{
					if (const auto t = source->getFloat_array())
					{
						const auto& transform = t->getValue();
						const auto n_transforms = transform.getCount() / 16;

						for (size_t k = 0; k < n_transforms; ++k)
						{
							LLMatrix4 mat;
							for (size_t i = 0; i < 4; ++i)
							{
								for (size_t j = 0; j < 4; ++j)
								{
									mat.mMatrix[i][j] = transform[k * 16 + i + j * 4];
								}
							}
							model->mSkinInfo.mInvBindMatrix.push_back(mat);
						}
					}
				}
			}
		}

		//Now that we've parsed the joint array, let's determine if we have a full rig
		//(which means we have all the joint sthat are required for an avatar versus
		//a skinned asset attached to a node in a file that contains an entire skeleton,
		//but does not use the skeleton).
		critiqueRigForUploadApplicability(model->mSkinInfo.mJointNames);

		if (!missingSkeletonOrScene)
		{
			// FIXME: mesh_id is used to determine which mesh gets to
			// set the joint offset, in the event of a conflict. Since
			// we don't know the mesh id yet, we can't guarantee that
			// joint offsets will be applied with the same priority as
			 // in the uploaded model. If the file contains multiple
			// meshes with conflicting joint offsets, preview may be
			// incorrect.
			LLUUID fake_mesh_id;
			fake_mesh_id.generate();

			//The joints are reset in the dtor
			//if ( getRigWithSceneParity() )
			{
				for (const auto& masterJointPair : mJointMap)
				{
					const auto lookingForJoint = masterJointPair.first;
					const auto& joint_found = mJointList.find(lookingForJoint);
					if (joint_found != mJointList.end())
					{
						//LL_INFOS()<<"joint "<<lookingForJoint.c_str()<<LL_ENDL;
						if (const auto pJoint = mJointLookupFunc(lookingForJoint, mOpaqueData))
						{
							const auto jointTransform = joint_found->second;
							const auto& joint_pos = jointTransform.getTranslation();
							if (pJoint->aboveJointPosThreshold(joint_pos))
							{
								bool override_changed; // not used
								pJoint->addAttachmentPosOverride(joint_pos, fake_mesh_id, "", override_changed);
								if (model->mSkinInfo.mLockScaleIfJointPosition)
								{
									pJoint->addAttachmentScaleOverride(pJoint->getDefaultScale(), fake_mesh_id, "");
								}
							}
						}
						else
						{
							//Most likely an error in the asset.
							LL_WARNS() << "Tried to apply joint position from .dae, but it did not exist in the avatar rig." << LL_ENDL;
						}
					}
				}
			}
		} //missingSkeletonOrScene

		//We need to construct the alternate bind matrix (which contains the new joint positions)
		//in the same order as they were stored in the joint buffer. The joints associated
		//with the skeleton are not stored in the same order as they are in the exported joint buffer.
		//This remaps the skeletal joints to be in the same order as the joints stored in the model.

		for (auto const& joint : model->mSkinInfo.mJointNames | boost::adaptors::indexed(0))
		{
			std::string lookingForJoint = joint.value().c_str();
			//Look for the joint xform that we extracted from the skeleton, using the jointIt as the key
			//and store it in the alternate bind matrix
			if (mJointMap.find(lookingForJoint) != mJointMap.end())
			{
				auto newInverse = model->mSkinInfo.mInvBindMatrix[joint.index()];
				newInverse.setTranslation(mJointList[lookingForJoint].getTranslation());
				model->mSkinInfo.mAlternateBindMatrix.push_back(newInverse);
			}
			else
			{
				LL_DEBUGS("Mesh") << "Possibly misnamed/missing joint [" << lookingForJoint.c_str() << "] " << LL_ENDL;
			}
		}

		//get raw position array
		if (auto verts = mesh->getVertices())
		{
			const auto& inputs = verts->getInput_array();
			const auto inputs_count = inputs.getCount();
			for (size_t i = 0; i < inputs_count && model->mPosition.empty(); ++i)
			{
				if (strcmp(inputs[i]->getSemantic(), COMMON_PROFILE_INPUT_POSITION) == 0)
				{
					if (const auto* pos_source = daeSafeCast<domSource>(inputs[i]->getSource().getElement()))
					{
						if (const auto pos_array = pos_source->getFloat_array())
						{
							const auto& pos = pos_array->getValue();
							const auto pos_count = pos.getCount();
							for (size_t j = 0; j < pos_count; j += 3)
							{
								if (pos_count <= j + 2)
								{
									LL_ERRS() << "Invalid position array size." << LL_ENDL;
								}

								LLVector3 v(pos[j], pos[j + 1], pos[j + 2]);

								//transform from COLLADA space to volume space
								v = v * inverse_normalized_transformation;

								model->mPosition.push_back(v);
							}
						}
					}
				}
			}
		}

		//get skin weights array
		if (auto weights = skin->getVertex_weights())
		{
			const auto& inputs = weights->getInput_array();
			domFloat_array* vertex_weights = NULL;
			for (size_t i = 0; i < inputs.getCount(); ++i)
			{
				if (strcmp(inputs[i]->getSemantic(), COMMON_PROFILE_INPUT_WEIGHT) == 0)
				{
					if (const auto weight_source = daeSafeCast<domSource>(inputs[i]->getSource().getElement()))
					{
						vertex_weights = weight_source->getFloat_array();
					}
				}
			}

			if (vertex_weights)
			{
				const auto& w = vertex_weights->getValue();
				const auto& vcount = weights->getVcount()->getValue();
				const auto vcount_count = vcount.getCount(); // sure ok
				const auto& v = weights->getV()->getValue();
				auto c_idx = 0;
				for (auto vc_idx = 0; vc_idx < vcount_count; ++vc_idx)
				{
					//for each vertex
					//create list of weights that influence this vertex
					LLModel::weight_list weight_list;

					const auto count = vcount[vc_idx];
					for (daeUInt i = 0; i < count; ++i)
					{
						//for each weight
						const auto joint_idx = v[c_idx++];
						const auto weight_idx = v[c_idx++];

						if (joint_idx == -1)
						{
							//ignore bindings to bind_shape_matrix
							continue;
						}

						const auto weight_value = (F32)w[weight_idx];

						weight_list.push_back(LLModel::JointWeight(joint_idx, weight_value));
					}

					//sort by joint weight
					std::sort(weight_list.begin(), weight_list.end(), LLModel::CompareWeightGreater());

					std::vector<LLModel::JointWeight> wght;

					auto total = 0.f;
					const auto n_weights = llmin(size_t(4), weight_list.size());
					for (size_t i = 0; i < n_weights; ++i)
					{
						//take up to 4 most significant weights
						const auto weight = weight_list[i];
						const auto weight_value = weight.mWeight;
						if (weight_value > 0.f)
						{
							wght.push_back(weight);
							total += weight_value;
						}
					}

					if (total != 1.f)
					{
						//normalize weights
						const auto scale = 1.f / total;
						for (size_t i = 0; i < wght.size(); ++i)
						{
							wght[i].mWeight *= scale;
						}
					}

					model->mSkinWeights[model->mPosition[vc_idx]] = wght;
				}
			}

		}
		//add instance to scene for this model

		LLMatrix4 transformation;
		transformation.initScale(mesh_scale_vector);
		transformation.setTranslation(mesh_translation_vector);
		transformation *= mTransform;

		material_map materials;
		for (const auto& m : model->mMaterialList)
		{
			materials[m] = LLImportMaterial();
		}
		mScene[transformation].push_back(LLModelInstance(model, model->mLabel, transformation, materials));
		stretch_extents(model, transformation, mExtents[0], mExtents[1], mFirstTransform);
	}
}

//-----------------------------------------------------------------------------
// buildJointToNodeMappingFromScene()
//-----------------------------------------------------------------------------
void LLDAELoader::buildJointToNodeMappingFromScene(daeElement* pRoot)
{
	const auto pScene = pRoot->getDescendant("visual_scene");
	if (pScene)
	{
		const auto children = pScene->getChildren();
		for (size_t i = 0; i < children.getCount(); ++i)
		{
			const auto pNode = daeSafeCast<domNode>(children[i]);
			processJointToNodeMapping(pNode);
		}
	}
}
//-----------------------------------------------------------------------------
// processJointToNodeMapping()
//-----------------------------------------------------------------------------
void LLDAELoader::processJointToNodeMapping(domNode* pNode)
{
	if (isNodeAJoint(pNode))
	{
		//1.Store the parent
		const auto nodeName = std::string(pNode->getName());
		if (!nodeName.empty())
		{
			mJointsFromNode.push_front(nodeName);
			// Alias joint node SIDs to joint names for compatibility
			const auto nodeSID = std::string(pNode->getSid());
			if (!nodeSID.empty())
				mJointMap[nodeSID] = mJointMap[nodeName];
		}
		//2. Handle the kiddo's
		processChildJoints(pNode);
	}
	else
	{
		//Determine if the're any children wrt to this failed node.
		//This occurs when an armature is exported and ends up being what essentially amounts to
		//as the root for the visual_scene
		if (pNode)
		{
			processChildJoints(pNode);
		}
		else
		{
			LL_INFOS() << "Node is NULL" << LL_ENDL;
		}

	}
}
//-----------------------------------------------------------------------------
// processChildJoint()
//-----------------------------------------------------------------------------
void LLDAELoader::processChildJoints(domNode* pParentNode)
{
	const auto childOfChild = pParentNode->getChildren();
	for (size_t i = 0; i < childOfChild.getCount(); ++i)
	{
		const auto pChildNode = daeSafeCast<domNode>(childOfChild[i]);
		if (pChildNode)
		{
			processJointToNodeMapping(pChildNode);
		}
	}
}

//-----------------------------------------------------------------------------
// isNodeAJoint()
//-----------------------------------------------------------------------------
bool LLDAELoader::isNodeAJoint(const domNode* pNode) const
{
	if (pNode)
	{
		if (const auto pNodeName = pNode->getName())
		{
			return LLModelLoader::isNodeAJoint(pNodeName);
		}
	}
	LL_INFOS() << "Created node is NULL or invalid" << LL_ENDL;
	return false;
}
//-----------------------------------------------------------------------------
// verifyCount
//-----------------------------------------------------------------------------
bool LLDAELoader::verifyCount(const size_t expected, const size_t result) const
{
	if (expected != result)
	{
		LL_INFOS() << "Error: (expected/got)" << expected << "/" << result << "verts" << LL_ENDL;
		return false;
	}
	return true;
}
//-----------------------------------------------------------------------------
// verifyController
//-----------------------------------------------------------------------------
bool LLDAELoader::verifyController(const domController* pController) const
{
	bool result = true;

	if (const auto pSkin = pController->getSkin())
	{
		const auto& uri = pSkin->getSource();
		const auto pElement = uri.getElement();

		if (!pElement)
		{
			LL_INFOS() << "Can't resolve skin source" << LL_ENDL;
			return false;
		}

		const auto type_str = pElement->getTypeName();
		if (stricmp(type_str, "geometry") == 0)
		{
			//Skin is reference directly by geometry and get the vertex count from skin
			const auto pVertexWeights = pSkin->getVertex_weights();
			const auto vertexWeightsCount = pVertexWeights->getCount();
			const auto pGeometry = (domGeometry*)(domElement*)uri.getElement();
			if (const auto pMesh = pGeometry->getMesh())
			{
				//Get vertex count from geometry
				if (const auto pVertices = pMesh->getVertices())
				{
					const auto src = pVertices->getInput_array()[0]->getSource();
					const auto pSource = (domSource*)(domElement*)src.getElement();
					const auto verticesCount = pSource->getTechnique_common()->getAccessor()->getCount();
					result = verifyCount(verticesCount, vertexWeightsCount);
					if (!result)
					{
						return false;
					}
				}
				else
				{
					LL_INFOS() << "No vertices!" << LL_ENDL;
					return false;
				}
			}

			const auto vcount_count = pVertexWeights->getVcount()->getValue().getCount();
			result = verifyCount(vcount_count, vertexWeightsCount);
			if (!result)
			{
				return false;
			}

			const auto& inputs = pVertexWeights->getInput_array();
			size_t sum = 0;
			for (size_t i = 0; i < vcount_count; ++i)
			{
				sum += pVertexWeights->getVcount()->getValue()[i];
			}
			result = verifyCount(sum * inputs.getCount(), (domInt)pVertexWeights->getV()->getValue().getCount());
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
// extractTranslation()
//-----------------------------------------------------------------------------
void LLDAELoader::extractTranslation(const domTranslate* pTranslate, LLMatrix4& transform) const
{
	const auto& jointTrans = pTranslate->getValue();
	LLVector3 singleJointTranslation(jointTrans[0], jointTrans[1], jointTrans[2]);
	transform.setTranslation(singleJointTranslation);
}

//-----------------------------------------------------------------------------
// extractTranslationViaSID()
//-----------------------------------------------------------------------------
void LLDAELoader::extractTranslationViaSID(daeElement* pElement, LLMatrix4& transform) const
{
	if (pElement)
	{
		daeSIDResolver resolver(pElement, "./transform");
		const auto pMatrix = daeSafeCast<domMatrix>(resolver.getElement());
		//We are only extracting out the translational component atm
		LLMatrix4 workingTransform;
		if (pMatrix)
		{
			const auto domArray = pMatrix->getValue();
			for (size_t i = 0; i < 4; ++i)
			{
				for (size_t j = 0; j < 4; ++j)
				{
					workingTransform.mMatrix[i][j] = domArray[i + j * 4];
				}
			}
			const auto trans = workingTransform.getTranslation();
			transform.setTranslation(trans);
		}
	}
	else
	{
		LL_WARNS() << "Element is nonexistent - empty/unsupported node." << LL_ENDL;
	}
}
//-----------------------------------------------------------------------------
// processJointNode()
//-----------------------------------------------------------------------------
void LLDAELoader::processJointNode(domNode* pNode, JointTransformMap& jointTransforms)
{
	if (pNode->getName() == NULL)
	{
		LL_WARNS() << "nameless node, can't process" << LL_ENDL;
		return;
	}

	//llwarns<<"ProcessJointNode# Node:" <<pNode->getName()<<LL_ENDL;

	//1. handle the incoming node - extract out translation via SID or element

	LLMatrix4 workingTransform;

	//Pull out the translate id and store it in the jointTranslations map
	daeSIDResolver jointResolverA(pNode, "./translate");
	daeSIDResolver jointResolverB(pNode, "./location");

	//Translation via SID was successful
	if (const auto pTranslateA = daeSafeCast<domTranslate>(jointResolverA.getElement()))
	{
		extractTranslation(pTranslateA, workingTransform);
	}
	else if (const auto pTranslateB = daeSafeCast<domTranslate>(jointResolverB.getElement()))
	{
		extractTranslation(pTranslateB, workingTransform);
	}
	else
	{
		const auto pTranslateElement = getChildFromElement(pNode, "translate");
		if (const auto pTranslateC = daeSafeCast<domTranslate>(pTranslateElement))
		{
			extractTranslation(pTranslateC, workingTransform);
		}
		else
		{
			daeSIDResolver jointResolver(pNode, "./matrix");
			if (const auto pMatrix = daeSafeCast<domMatrix>(jointResolver.getElement()))
			{
				//LL_INFOS() <<"A matrix SID was however found!"<<LL_ENDL;
				const auto domArray = pMatrix->getValue();
				for (size_t i = 0; i < 4; ++i)
				{
					for (size_t j = 0; j < 4; ++j)
					{
						workingTransform.mMatrix[i][j] = domArray[i + j * 4];
					}
				}
			}
			else
			{
				LL_WARNS() << "The found element is not translate or matrix node - most likely a corrupt export!" << LL_ENDL;
			}
		}
	}

	//Store the working transform relative to the nodes name.
	jointTransforms[pNode->getName()] = workingTransform;

	//2. handle the nodes children

	//Gather and handle the incoming nodes children
	const auto childOfChild = pNode->getChildren();
	const auto childOfChildCount = childOfChild.getCount();

	for (size_t i = 0; i < childOfChildCount; ++i)
	{
		if (const auto pChildNode = daeSafeCast<domNode>(childOfChild[i]))
		{
			processJointNode(pChildNode, jointTransforms);
		}
	}
}

//-----------------------------------------------------------------------------
// getChildFromElement()
//-----------------------------------------------------------------------------
daeElement* LLDAELoader::getChildFromElement(daeElement* pElement, std::string const& name)
{
	daeElement* pChildOfElement = pElement->getChild(name.c_str());
	if (pChildOfElement)
	{
		return pChildOfElement;
	}
	LL_DEBUGS("Mesh") << "Could not find a child [" << name << "] for the element: \"" << pElement->getAttribute("id") << "\"" << LL_ENDL;
	return NULL;
}

void LLDAELoader::processElement(daeElement* element, bool& badElement, DAE* dae, daeElement* domRoot)
{
	LLMatrix4 saved_transform, saved_bind_transform;
	bool pushed_mat = false;

	if (const auto node = daeSafeCast<domNode>(element))
	{
		pushed_mat = true;
		saved_transform = mTransform;
		saved_bind_transform = mBindTransform;
	}

	if (const auto translate = daeSafeCast<domTranslate>(element))
	{
		const auto dom_value = translate->getValue();

		LLMatrix4 translation, translation2;
		translation.setTranslation(LLVector3(dom_value[0], dom_value[1], dom_value[2]));
		translation2 = translation;

		translation *= mTransform;
		mTransform = translation;
		mTransform.condition();

		translation2 *= mBindTransform;
		mBindTransform = translation2;
		mBindTransform.condition();
	}

	if (const auto rotate = daeSafeCast<domRotate>(element))
	{
		const auto dom_value = rotate->getValue();

		LLMatrix4 rotation, rotation2;
		rotation.initRotTrans(dom_value[3] * DEG_TO_RAD, LLVector3(dom_value[0], dom_value[1], dom_value[2]), LLVector3(0, 0, 0));
		rotation2 = rotation;

		rotation *= mTransform;
		mTransform = rotation;
		mTransform.condition();

		rotation2 *= mBindTransform;
		mBindTransform = rotation2;
		mBindTransform.condition();
	}

	if (const auto scale = daeSafeCast<domScale>(element))
	{
		const auto dom_value = scale->getValue();

		auto scale_vector = LLVector3(dom_value[0], dom_value[1], dom_value[2]);
		scale_vector.abs(); // Set all values positive, since we don't currently support mirrored meshes
		LLMatrix4 scaling, scaling2;
		scaling.initScale(scale_vector);
		scaling2 = scaling;

		scaling *= mTransform;
		mTransform = scaling;
		mTransform.condition();

		scaling2 *= mBindTransform;
		mBindTransform = scaling2;
		mBindTransform.condition();
	}

	if (const auto matrix = daeSafeCast<domMatrix>(element))
	{
		const auto dom_value = matrix->getValue();

		LLMatrix4 matrix_transform, matrix_transform2;

		for (size_t i = 0; i < 4; ++i)
		{
			for (size_t j = 0; j < 4; ++j)
			{
				matrix_transform.mMatrix[i][j] = dom_value[i + j * 4];
			}
		}

		matrix_transform2 = matrix_transform;

		matrix_transform *= mTransform;
		mTransform = matrix_transform;
		mTransform.condition();

		matrix_transform2 *= mBindTransform;
		mBindTransform = matrix_transform2;
		mBindTransform.condition();
	}

	// Process instance_geometry for static meshes
	if (const auto instance_geo = daeSafeCast<domInstance_geometry>(element))
	{
		if (const auto geo = daeSafeCast<domGeometry>(instance_geo->getUrl().getElement()))
		{
			if (const auto mesh = daeSafeCast<domMesh>(geo->getDescendant(daeElement::matchType(domMesh::ID()))))
			{
				for (auto& model : mModelsMap[mesh])
				{
					auto transformation = mTransform;

					if (mTransform.determinant() < 0)
					{
						//negative scales are not supported
						LL_INFOS() << "Negative scale detected, unsupported transform.  domInstance_geometry: " << getElementLabel(instance_geo) << LL_ENDL;
						badElement = true;
					}

					auto materials = getMaterials(model, instance_geo, dae);

					// adjust the transformation to compensate for mesh normalization
					LLVector3 mesh_scale_vector;
					LLVector3 mesh_translation_vector;
					model->getNormalizedScaleTranslation(mesh_scale_vector, mesh_translation_vector);

					LLMatrix4 mesh_translation;
					mesh_translation.setTranslation(mesh_translation_vector);
					mesh_translation *= transformation;
					transformation = mesh_translation;

					LLMatrix4 mesh_scale;
					mesh_scale.initScale(mesh_scale_vector);
					mesh_scale *= transformation;
					transformation = mesh_scale;

					if (transformation.determinant() < 0)
					{
						//negative scales are not supported
						LL_INFOS() << "Negative scale detected, unsupported post-normalization transform.  domInstance_geometry: " << getElementLabel(instance_geo) << LL_ENDL;
						badElement = true;
					}

					std::string label;

					if (model->mLabel.empty())
					{
						label = getLodlessLabel(instance_geo);

						llassert(!label.empty());

						if (model->mSubmodelID)
						{
							label += (char)((int)'a' + model->mSubmodelID);
						}

						model->mLabel = label + lod_suffix[mLod];
					}
					else
					{
						// Don't change model's name if possible, it will play havoc with scenes that already use said model.
						const auto ext_pos = getSuffixPosition(model->mLabel);
						if (ext_pos != -1)
						{
							label = model->mLabel.substr(0, ext_pos);
						}
						else
						{
							label = model->mLabel;
						}
					}

					mScene[transformation].push_back(LLModelInstance(model, label, transformation, materials));
					stretch_extents(model, transformation, mExtents[0], mExtents[1], mFirstTransform);
				}
			}
		}
		else
		{
			LL_INFOS() << "Unable to resolve geometry URL." << LL_ENDL;
			badElement = true;
		}
	}

	// Process instance_control elements for skinned meshes
	if (const auto instance_ctl = daeSafeCast<domInstance_controller>(element))
	{
		if (const auto ctl = daeSafeCast<domController>(instance_ctl->getUrl().getElement()))
		{
			if (const auto skin = ctl->getSkin())
			{
				if (const auto geom = daeSafeCast<domGeometry>(skin->getSource().getElement()))
				{
					if (const auto mesh = geom->getMesh())
					{
						for (const auto mdl : mModelsMap[mesh])
						{
							LLDAELoader::processDomModel(mdl, dae, domRoot, mesh, skin);
						}
					}
				}
			}
		}
	}

	// Resolve nodes to instances
	if (const auto instance_node = daeSafeCast<domInstance_node>(element))
	{
		if (const auto instance = instance_node->getUrl().getElement())
		{
			processElement(instance, badElement, dae, domRoot);
		}
	}

	//process children
	const auto children = element->getChildren();
	for (size_t i = 0; i < children.getCount(); ++i)
	{
		processElement(children[i], badElement, dae, domRoot);
	}

	if (pushed_mat)
	{
		//this element was a node, restore transform before processiing siblings
		mTransform = saved_transform;
		mBindTransform = saved_bind_transform;
	}
}

std::map<std::string, LLImportMaterial> LLDAELoader::getMaterials(LLModel* model, domInstance_geometry* instance_geo, DAE* dae) const
{
	std::map<std::string, LLImportMaterial> materials;
	for (const auto& material : model->mMaterialList)
	{
		LLImportMaterial import_material;
		domInstance_material* instance_mat = NULL;

		if (const auto technique = daeSafeCast<domBind_material::domTechnique_common>(
			instance_geo->getDescendant(daeElement::matchType(domBind_material::domTechnique_common::ID()))))
		{
			const auto inst_materials = technique->getChildrenByType<domInstance_material>();
			for (size_t j = 0; j < inst_materials.getCount(); ++j)
			{
				std::string symbol(inst_materials[j]->getSymbol());

				if (symbol == material) // found the binding
				{
					instance_mat = inst_materials[j];
					break;
				}
			}
		}

		if (instance_mat)
		{
			if (const auto material = daeSafeCast<domMaterial>(instance_mat->getTarget().getElement()))
			{
				if (const auto instance_effect = daeSafeCast<domInstance_effect>(
					material->getDescendant(daeElement::matchType(domInstance_effect::ID()))))
				{
					if (const auto effect = daeSafeCast<domEffect>(
						instance_effect->getUrl().getElement()))
					{
						if (const auto profile = daeSafeCast<domProfile_COMMON>(
							effect->getDescendant(daeElement::matchType(domProfile_COMMON::ID()))))
						{
							import_material = profileToMaterial(profile, dae);
						}
					}
				}
			}
		}

		import_material.mBinding = material;
		materials[material] = import_material;
	}

	return materials;
}

LLImportMaterial LLDAELoader::profileToMaterial(domProfile_COMMON* material, DAE* dae) const
{
	LLImportMaterial mat;
	mat.mFullbright = FALSE;

	if (const auto diffuse = material->getDescendant("diffuse"))
	{
		if (const auto texture = daeSafeCast<domCommon_color_or_texture_type_complexType::domTexture>(diffuse->getDescendant("texture")))
		{
			const auto& newparams = material->getNewparam_array();
			if (newparams.getCount())
			{
				for (size_t i = 0; i < newparams.getCount(); ++i)
				{
					if (const auto surface = newparams[i]->getSurface())
					{
						if (const auto init = surface->getFx_surface_init_common())
						{
							const auto init_from = init->getInit_from_array();
							if (init_from.getCount() > i)
							{
								if (const auto image = daeSafeCast<domImage>(init_from[i]->getValue().getElement()))
								{
									// we only support init_from now - embedded data will come later
									if (const auto init = image->getInit_from())
									{
										mat.mDiffuseMapFilename = cdom::uriToNativePath(init->getValue().str());
										mat.mDiffuseMapLabel = getElementLabel(material);
									}
								}
							}
						}
					}
				}
			}
			else if (texture->getTexture())
			{
				domImage* image = NULL;
				dae->getDatabase()->getElement((daeElement**)&image, 0, texture->getTexture(), COLLADA_TYPE_IMAGE);
				if (image)
				{
					// we only support init_from now - embedded data will come later
					if (const auto init = image->getInit_from())
					{
						const auto image_path_value = cdom::uriToNativePath(init->getValue().str());

#if LL_WINDOWS
						// Work-around DOM tendency to resort to UNC names which are only confusing for downstream...
						//
						auto i = image_path_value.cbegin();
						while (*i == '\\')
							++i;

						mat.mDiffuseMapFilename.assign(i, image_path_value.end());
#else
						mat.mDiffuseMapFilename = image_path_value;
#endif
						mat.mDiffuseMapLabel = getElementLabel(material);
					}
				}
			}
		}

		if (const auto color = daeSafeCast<domCommon_color_or_texture_type_complexType::domColor>(diffuse->getDescendant("color")))
		{
			const auto domfx_color = color->getValue();
			const auto value = LLColor4(domfx_color[0], domfx_color[1], domfx_color[2], domfx_color[3]);
			mat.mDiffuseColor = value;
		}
	}

	if (const auto emission = material->getDescendant("emission"))
	{
		const auto emission_color = getDaeColor(emission);
		const auto color_avg = (emission_color[0] + emission_color[1] + emission_color[2]) / 3.0f;
		mat.mFullbright |= color_avg > 0.25f;
	}

	return mat;
}

// try to get a decent label for this element
std::string LLDAELoader::getElementLabel(daeElement* element)
{
	// if we have a name attribute, use it
	std::string name = element->getAttribute("name");
	if (name.length())
	{
		return name;
	}

	// if we have an ID attribute, use it
	if (element->getID())
	{
		return std::string(element->getID());
	}

	// if we have a parent, use it
	const auto parent = element->getParent();
	std::string index_string;
	if (parent)
	{
		// retrieve index to distinguish items inside same parent
		auto ind = size_t(0);
		parent->getChildren().find(element, ind);

		if (ind > 0)
		{
			index_string = "_" + fmt::to_string(ind);
		}

		// if parent has a name or ID, use it
		auto name = parent->getAttribute("name");
		if (!name.length())
		{
			name = std::string(parent->getID());
		}

		if (name.length())
		{
			// make sure that index won't mix up with pre-named lod extensions
			const auto ext_pos = getSuffixPosition(name);

			if (ext_pos == -1)
			{
				return name + index_string;
			}
			else
			{
				return name.insert(ext_pos, index_string);
			}
		}
	}

	// try to use our type
	const auto element_name = element->getElementName();
	if (element_name)
	{
		return std::string(element_name) + index_string;
	}

	// if all else fails, use "object"
	return std::string("object") + index_string;
}

// static
size_t LLDAELoader::getSuffixPosition(const std::string label)
{
	if ((label.find("_LOD") != std::string::npos) || (label.find("_PHYS") != std::string::npos))
	{
		return label.rfind('_');
	}
	return -1;
}

// static
std::string LLDAELoader::getLodlessLabel(daeElement* element)
{
	std::string label = getElementLabel(element);
	size_t ext_pos = getSuffixPosition(label);
	if (ext_pos != -1)
	{
		return label.substr(0, ext_pos);
	}
	return label;
}

LLColor4 LLDAELoader::getDaeColor(daeElement* element) const
{
	LLColor4 value;
	if (const auto color = daeSafeCast<domCommon_color_or_texture_type_complexType::domColor>(element->getDescendant("color")))
	{
		const auto domfx_color = color->getValue();
		value = LLColor4(domfx_color[0], domfx_color[1], domfx_color[2], domfx_color[3]);
	}

	return value;
}

bool LLDAELoader::addVolumeFacesFromDomMesh(LLModel* pModel, domMesh* mesh)
{
	auto status = LLModel::NO_ERRORS;
	auto& tris = mesh->getTriangles_array();
	for (size_t i = 0; i < tris.getCount(); ++i)
	{
		auto& tri = tris.get(i);
		status = load_face_from_dom_triangles(pModel->getVolumeFaces(), pModel->getMaterialList(), tri);
		pModel->mStatus = status;
		if (status != LLModel::NO_ERRORS)
		{
			pModel->ClearFacesAndMaterials();
			return false;
		}
	}

	auto& polys = mesh->getPolylist_array();
	for (size_t i = 0; i < polys.getCount(); ++i)
	{
		auto& poly = polys.get(i);
		status = load_face_from_dom_polylist(pModel->getVolumeFaces(), pModel->getMaterialList(), poly);
		if (status != LLModel::NO_ERRORS)
		{
			pModel->ClearFacesAndMaterials();
			return false;
		}
	}

	auto& polygons = mesh->getPolygons_array();
	for (size_t i = 0; i < polygons.getCount(); ++i)
	{
		auto& poly = polygons.get(i);
		status = load_face_from_dom_polygons(pModel->getVolumeFaces(), pModel->getMaterialList(), poly);
		if (status != LLModel::NO_ERRORS)
		{
			pModel->ClearFacesAndMaterials();
			return false;
		}
	}

	return (status == LLModel::NO_ERRORS);
}

//static 
LLModel* LLDAELoader::loadModelFromDomMesh(domMesh* mesh)
{
	LLVolumeParams volume_params;
	volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
	auto ret = new LLModel(volume_params, 0.f);
	createVolumeFacesFromDomMesh(ret, mesh);
	if (ret->mLabel.empty())
	{
		ret->mLabel = getElementLabel(mesh);
	}
	return ret;
}

//static diff version supports creating multiple models when material counts spill
// over the 8 face server-side limit
//
bool LLDAELoader::loadModelsFromDomMesh(domMesh* mesh, std::vector<LLModel*>& models_out, U32 submodel_limit)
{

	LLVolumeParams volume_params;
	volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);

	models_out.clear();

	auto ret = new LLModel(volume_params, 0.f);

	const auto model_name = getLodlessLabel(mesh);
	ret->mLabel = model_name + lod_suffix[mLod];

	llassert(!ret->mLabel.empty());

	// Like a monkey, ready to be shot into space
	//
	ret->ClearFacesAndMaterials();

	// Get the whole set of volume faces
	//
	addVolumeFacesFromDomMesh(ret, mesh);

	auto volume_faces = (U32)ret->getNumVolumeFaces();

	// Side-steps all manner of issues when splitting models
	// and matching lower LOD materials to base models
	//
	ret->sortVolumeFacesByMaterialName();

	bool normalized = false;

	auto submodelID = 0;

	// remove all faces that definitely won't fit into one model and submodel limit
	const auto face_limit = (submodel_limit + 1) * LL_SCULPT_MESH_MAX_FACES;
	if (face_limit < volume_faces)
	{
		ret->setNumVolumeFaces(face_limit);
	}

	LLVolume::face_list_t remainder;
	do
	{
		// Insure we do this once with the whole gang and not per-model
		//
		if (!normalized && !mNoNormalize)
		{
			normalized = true;
			ret->normalizeVolumeFaces();
		}

		ret->trimVolumeFacesToSize(LL_SCULPT_MESH_MAX_FACES, &remainder);

		if (!mNoOptimize)
		{
			ret->optimizeVolumeFaces();
		}

		volume_faces = remainder.size();

		models_out.push_back(ret);

		// If we have left-over volume faces, create another model
		// to absorb them...
		//
		if (volume_faces)
		{
			auto next = new LLModel(volume_params, 0.f);
			next->mSubmodelID = ++submodelID;
			next->mLabel = model_name + (char)((int)'a' + next->mSubmodelID) + lod_suffix[mLod];
			next->getVolumeFaces() = remainder;
			next->mNormalizedScale = ret->mNormalizedScale;
			next->mNormalizedTranslation = ret->mNormalizedTranslation;
			if (ret->mMaterialList.size() > LL_SCULPT_MESH_MAX_FACES)
			{
				next->mMaterialList.assign(ret->mMaterialList.begin() + LL_SCULPT_MESH_MAX_FACES, ret->mMaterialList.end());
			}
			ret = next;
		}

		remainder.clear();

	} while (volume_faces);

	return true;
}

bool LLDAELoader::createVolumeFacesFromDomMesh(LLModel* pModel, domMesh* mesh)
{
	if (mesh)
	{
		pModel->ClearFacesAndMaterials();

		addVolumeFacesFromDomMesh(pModel, mesh);

		if (pModel->getNumVolumeFaces() > 0)
		{
			pModel->normalizeVolumeFaces();
			pModel->optimizeVolumeFaces();

			if (pModel->getNumVolumeFaces() > 0)
			{
				return true;
			}
		}
	}
	else
	{
		LL_WARNS() << "no mesh found" << LL_ENDL;
	}

	return false;
}

